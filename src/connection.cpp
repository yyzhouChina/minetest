/*
Minetest
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "connection.h"
#include "main.h"
#include "serialization.h"
#include "log.h"
#include "porting.h"
#include "util/serialize.h"
#include "util/numeric.h"
#include "util/string.h"
#include "settings.h"

namespace con
{

JMutex log_message_mutex;

#define LOG(a)                                                                 \
	{                                                                          \
	JMutexAutoLock loglock(log_message_mutex);                                 \
	a;                                                                         \
	}


static u16 readPeerId(u8 *packetdata)
{
	return readU16(&packetdata[4]);
}
static u8 readChannel(u8 *packetdata)
{
	return readU8(&packetdata[6]);
}

BufferedPacket makePacket(Address &address, u8 *data, u32 datasize,
		u32 protocol_id, u16 sender_peer_id, u8 channel)
{
	u32 packet_size = datasize + BASE_HEADER_SIZE;
	BufferedPacket p(packet_size);
	p.address = address;

	writeU32(&p.data[0], protocol_id);
	writeU16(&p.data[4], sender_peer_id);
	writeU8(&p.data[6], channel);

	memcpy(&p.data[BASE_HEADER_SIZE], data, datasize);

	return p;
}

BufferedPacket makePacket(Address &address, SharedBuffer<u8> &data,
		u32 protocol_id, u16 sender_peer_id, u8 channel)
{
	return makePacket(address, *data, data.getSize(),
			protocol_id, sender_peer_id, channel);
}

SharedBuffer<u8> makeOriginalPacket(
		SharedBuffer<u8> data)
{
	u32 header_size = 1;
	u32 packet_size = data.getSize() + header_size;
	SharedBuffer<u8> b(packet_size);

	writeU8(&b[0], TYPE_ORIGINAL);

	memcpy(&b[header_size], *data, data.getSize());

	return b;
}

std::list<SharedBuffer<u8> > makeSplitPacket(
		SharedBuffer<u8> data,
		u32 chunksize_max,
		u16 seqnum)
{
	// Chunk packets, containing the TYPE_SPLIT header
	std::list<SharedBuffer<u8> > chunks;
	
	u32 chunk_header_size = 7;
	u32 maximum_data_size = chunksize_max - chunk_header_size;
	u32 start = 0;
	u32 end = 0;
	u32 chunk_num = 0;
	u16 chunk_count = 0;
	do{
		end = start + maximum_data_size - 1;
		if(end > data.getSize() - 1)
			end = data.getSize() - 1;
		
		u32 payload_size = end - start + 1;
		u32 packet_size = chunk_header_size + payload_size;

		SharedBuffer<u8> chunk(packet_size);
		
		writeU8(&chunk[0], TYPE_SPLIT);
		writeU16(&chunk[1], seqnum);
		// [3] u16 chunk_count is written at next stage
		writeU16(&chunk[5], chunk_num);
		memcpy(&chunk[chunk_header_size], &data[start], payload_size);

		chunks.push_back(chunk);
		chunk_count++;
		
		start = end + 1;
		chunk_num++;
	}
	while(end != data.getSize() - 1);

	for(std::list<SharedBuffer<u8> >::iterator i = chunks.begin();
		i != chunks.end(); ++i)
	{
		// Write chunk_count
		writeU16(&((*i)[3]), chunk_count);
	}

	return chunks;
}

std::list<SharedBuffer<u8> > makeAutoSplitPacket(
		SharedBuffer<u8> data,
		u32 chunksize_max,
		u16 &split_seqnum)
{
	u32 original_header_size = 1;
	std::list<SharedBuffer<u8> > list;
	if(data.getSize() + original_header_size > chunksize_max)
	{
		list = makeSplitPacket(data, chunksize_max, split_seqnum);
		split_seqnum++;
		return list;
	}
	else
	{
		list.push_back(makeOriginalPacket(data));
	}
	return list;
}

SharedBuffer<u8> makeReliablePacket(
		SharedBuffer<u8> data,
		u16 seqnum)
{
	/*dstream<<"BEGIN SharedBuffer<u8> makeReliablePacket()"<<std::endl;
	dstream<<"data.getSize()="<<data.getSize()<<", data[0]="
			<<((unsigned int)data[0]&0xff)<<std::endl;*/
	u32 header_size = 3;
	u32 packet_size = data.getSize() + header_size;
	SharedBuffer<u8> b(packet_size);

	writeU8(&b[0], TYPE_RELIABLE);
	writeU16(&b[1], seqnum);

	memcpy(&b[header_size], *data, data.getSize());

	/*dstream<<"data.getSize()="<<data.getSize()<<", data[0]="
			<<((unsigned int)data[0]&0xff)<<std::endl;*/
	//dstream<<"END SharedBuffer<u8> makeReliablePacket()"<<std::endl;
	return b;
}

/*
	ReliablePacketBuffer
*/

ReliablePacketBuffer::ReliablePacketBuffer(): m_list_size(0) {}

void ReliablePacketBuffer::print()
{
	JMutexAutoLock listlock(m_list_mutex);
	for(std::list<BufferedPacket>::iterator i = m_list.begin();
		i != m_list.end();
		++i)
	{
		u16 s = readU16(&(i->data[BASE_HEADER_SIZE+1]));
		LOG(dout_con<<s<<" ");
	}
}
bool ReliablePacketBuffer::empty()
{
	JMutexAutoLock listlock(m_list_mutex);
	return m_list.empty();
}

u32 ReliablePacketBuffer::size()
{
	return m_list_size;
}

bool ReliablePacketBuffer::containsPacket(u16 seqnum)
{
	return !(findPacket(seqnum) == m_list.end());
}

RPBSearchResult ReliablePacketBuffer::findPacket(u16 seqnum)
{
	std::list<BufferedPacket>::iterator i = m_list.begin();
	for(; i != m_list.end(); ++i)
	{
		u16 s = readU16(&(i->data[BASE_HEADER_SIZE+1]));
		/*dout_con<<"findPacket(): finding seqnum="<<seqnum
				<<", comparing to s="<<s<<std::endl;*/
		if(s == seqnum)
			break;
	}
	return i;
}
RPBSearchResult ReliablePacketBuffer::notFound()
{
	return m_list.end();
}
bool ReliablePacketBuffer::getFirstSeqnum(u16 *result)
{
	JMutexAutoLock listlock(m_list_mutex);
	if(m_list.empty())
		return false;
	BufferedPacket p = *m_list.begin();
	*result = readU16(&p.data[BASE_HEADER_SIZE+1]);
	return true;
}
BufferedPacket ReliablePacketBuffer::popFirst()
{
	JMutexAutoLock listlock(m_list_mutex);
	if(m_list.empty())
		throw NotFoundException("Buffer is empty");
	BufferedPacket p = *m_list.begin();
	m_list.erase(m_list.begin());
	--m_list_size;
	return p;
}
BufferedPacket ReliablePacketBuffer::popSeqnum(u16 seqnum)
{
	JMutexAutoLock listlock(m_list_mutex);
	RPBSearchResult r = findPacket(seqnum);
	if(r == notFound()){
		LOG(dout_con<<"Sequence number: " << seqnum << " not found in reliable buffer"<<std::endl);
		throw NotFoundException("seqnum not found in buffer");
	}
	BufferedPacket p = *r;
	m_list.erase(r);
	--m_list_size;
	return p;
}
void ReliablePacketBuffer::insert(BufferedPacket &p)
{
	JMutexAutoLock listlock(m_list_mutex);
	assert(p.data.getSize() >= BASE_HEADER_SIZE+3);
	u8 type = readU8(&p.data[BASE_HEADER_SIZE+0]);
	assert(type == TYPE_RELIABLE);
	u16 seqnum = readU16(&p.data[BASE_HEADER_SIZE+1]);

	++m_list_size;
	// Find the right place for the packet and insert it there

	// If list is empty, just add it
	if(m_list.empty())
	{
		m_list.push_back(p);
		// Done.
		return;
	}
	// Otherwise find the right place
	std::list<BufferedPacket>::iterator i = m_list.begin();
	// Find the first packet in the list which has a higher seqnum
	for(; i != m_list.end(); ++i){
		u16 s = readU16(&(i->data[BASE_HEADER_SIZE+1]));
		if(s == seqnum){
			--m_list_size;
			throw AlreadyExistsException("Same seqnum in list");
		}
		if(seqnum_higher(s, seqnum)){
			break;
		}
	}
	// If we're at the end of the list, add the packet to the
	// end of the list
	if(i == m_list.end())
	{
		m_list.push_back(p);
		// Done.
		return;
	}
	// Insert before i
	m_list.insert(i, p);
}

void ReliablePacketBuffer::incrementTimeouts(float dtime)
{
	JMutexAutoLock listlock(m_list_mutex);
	for(std::list<BufferedPacket>::iterator i = m_list.begin();
		i != m_list.end(); ++i)
	{
		i->time += dtime;
		i->totaltime += dtime;
	}
}

void ReliablePacketBuffer::resetTimedOuts(float timeout)
{
	JMutexAutoLock listlock(m_list_mutex);
	for(std::list<BufferedPacket>::iterator i = m_list.begin();
		i != m_list.end(); ++i)
	{
		if(i->time >= timeout)
			i->time = 0.0;
	}
}

bool ReliablePacketBuffer::anyTotaltimeReached(float timeout)
{
	JMutexAutoLock listlock(m_list_mutex);
	for(std::list<BufferedPacket>::iterator i = m_list.begin();
		i != m_list.end(); ++i)
	{
		if(i->totaltime >= timeout)
			return true;
	}
	return false;
}

std::list<BufferedPacket> ReliablePacketBuffer::getTimedOuts(float timeout)
{
	JMutexAutoLock listlock(m_list_mutex);
	std::list<BufferedPacket> timed_outs;
	for(std::list<BufferedPacket>::iterator i = m_list.begin();
		i != m_list.end(); ++i)
	{
		if(i->time >= timeout)
			timed_outs.push_back(*i);
	}
	return timed_outs;
}

/*
	IncomingSplitBuffer
*/

IncomingSplitBuffer::~IncomingSplitBuffer()
{
	JMutexAutoLock listlock(m_map_mutex);
	for(std::map<u16, IncomingSplitPacket*>::iterator i = m_buf.begin();
		i != m_buf.end(); ++i)
	{
		delete i->second;
	}
}
/*
	This will throw a GotSplitPacketException when a full
	split packet is constructed.
*/
SharedBuffer<u8> IncomingSplitBuffer::insert(BufferedPacket &p, bool reliable)
{
	JMutexAutoLock listlock(m_map_mutex);
	u32 headersize = BASE_HEADER_SIZE + 7;
	assert(p.data.getSize() >= headersize);
	u8 type = readU8(&p.data[BASE_HEADER_SIZE+0]);
	assert(type == TYPE_SPLIT);
	u16 seqnum = readU16(&p.data[BASE_HEADER_SIZE+1]);
	u16 chunk_count = readU16(&p.data[BASE_HEADER_SIZE+3]);
	u16 chunk_num = readU16(&p.data[BASE_HEADER_SIZE+5]);

	// Add if doesn't exist
	if(m_buf.find(seqnum) == m_buf.end())
	{
		IncomingSplitPacket *sp = new IncomingSplitPacket();
		sp->chunk_count = chunk_count;
		sp->reliable = reliable;
		m_buf[seqnum] = sp;
	}
	
	IncomingSplitPacket *sp = m_buf[seqnum];
	
	// TODO: These errors should be thrown or something? Dunno.
	if(chunk_count != sp->chunk_count)
		LOG(derr_con<<"Connection: WARNING: chunk_count="<<chunk_count
				<<" != sp->chunk_count="<<sp->chunk_count
				<<std::endl);
	if(reliable != sp->reliable)
		LOG(derr_con<<"Connection: WARNING: reliable="<<reliable
				<<" != sp->reliable="<<sp->reliable
				<<std::endl);

	// If chunk already exists, ignore it.
	// Sometimes two identical packets may arrive when there is network
	// lag and the server re-sends stuff.
	if(sp->chunks.find(chunk_num) != sp->chunks.end())
		return SharedBuffer<u8>();
	
	// Cut chunk data out of packet
	u32 chunkdatasize = p.data.getSize() - headersize;
	SharedBuffer<u8> chunkdata(chunkdatasize);
	memcpy(*chunkdata, &(p.data[headersize]), chunkdatasize);
	
	// Set chunk data in buffer
	sp->chunks[chunk_num] = chunkdata;
	
	// If not all chunks are received, return empty buffer
	if(sp->allReceived() == false)
		return SharedBuffer<u8>();

	// Calculate total size
	u32 totalsize = 0;
	for(std::map<u16, SharedBuffer<u8> >::iterator i = sp->chunks.begin();
		i != sp->chunks.end(); ++i)
	{
		totalsize += i->second.getSize();
	}
	
	SharedBuffer<u8> fulldata(totalsize);

	// Copy chunks to data buffer
	u32 start = 0;
	for(u32 chunk_i=0; chunk_i<sp->chunk_count;
			chunk_i++)
	{
		SharedBuffer<u8> buf = sp->chunks[chunk_i];
		u16 chunkdatasize = buf.getSize();
		memcpy(&fulldata[start], *buf, chunkdatasize);
		start += chunkdatasize;;
	}

	// Remove sp from buffer
	m_buf.erase(seqnum);
	delete sp;

	return fulldata;
}
void IncomingSplitBuffer::removeUnreliableTimedOuts(float dtime, float timeout)
{
	std::list<u16> remove_queue;
	{
		JMutexAutoLock listlock(m_map_mutex);
		for(std::map<u16, IncomingSplitPacket*>::iterator i = m_buf.begin();
			i != m_buf.end(); ++i)
		{
			IncomingSplitPacket *p = i->second;
			// Reliable ones are not removed by timeout
			if(p->reliable == true)
				continue;
			p->time += dtime;
			if(p->time >= timeout)
				remove_queue.push_back(i->first);
		}
	}
	for(std::list<u16>::iterator j = remove_queue.begin();
		j != remove_queue.end(); ++j)
	{
		LOG(dout_con<<"NOTE: Removing timed out unreliable split packet"<<std::endl);
		delete m_buf[*j];
		m_buf.erase(*j);
	}
}

/*
	Channel
*/

#define MAX_RELIABLE_WINDOW_SIZE 1024
#define MIN_RELIABLE_WINDOW_SIZE   64

Channel::Channel()
{
	next_outgoing_seqnum = SEQNUM_INITIAL;
	next_incoming_seqnum = SEQNUM_INITIAL;
	next_outgoing_split_seqnum = SEQNUM_INITIAL;
	window_size = MAX_RELIABLE_WINDOW_SIZE;
	max_bpm = 0;
}

Channel::~Channel()
{
}

u16 Channel::getSequenceNumber()
{
	u16 retval = next_outgoing_seqnum;

	while(outgoing_reliables_sent.containsPacket(retval))
	{
		next_outgoing_seqnum++;
		retval = next_outgoing_seqnum;
	}
	next_outgoing_seqnum++;
	return retval;
}

void Channel::UpdateBytesSent(unsigned int bytes)
{
	JMutexAutoLock internal(m_internal_mutex);
	current_bytes_transfered += bytes;
}


void Channel::UpdatePacketLossCounter(unsigned int count)
{
	JMutexAutoLock internal(m_internal_mutex);
	current_packet_loss += count;
}

void Channel::UpdateTimers(float dtime)
{
	bpm_counter += dtime;
	packet_loss_counter += dtime;

	if (bpm_counter > 5.0)
	{
		bpm_counter -= 5.0;

		unsigned int packet_loss = 11; /* use a neutral value for initialization */

		{
			JMutexAutoLock internal(m_internal_mutex);
			packet_loss = current_packet_loss;
			current_packet_loss = 0;
		}
		/* TODO evaluate different parameters for this */
		if ((packet_loss == 0) &&
			(window_size < MAX_RELIABLE_WINDOW_SIZE))
		{
			window_size = MYMAX(
										(window_size + 10),
										MAX_RELIABLE_WINDOW_SIZE);
		}
		else if ((packet_loss < 10) &&
				(window_size < MAX_RELIABLE_WINDOW_SIZE))
		{
			window_size = MYMAX(
										(window_size + 2),
										MAX_RELIABLE_WINDOW_SIZE);
		}
		else if (packet_loss > 20)
		{
			window_size = MYMAX(
										(window_size - 2),
										MIN_RELIABLE_WINDOW_SIZE);
		}
		else if (packet_loss > 50)
		{
			window_size = MYMAX(
										(window_size - 10),
										MIN_RELIABLE_WINDOW_SIZE);
		}
	}

	if (bpm_counter > 60.0)
	{
		float pps = 0;

		{
			JMutexAutoLock internal(m_internal_mutex);
			pps = current_bytes_transfered/bpm_counter;
			bpm_counter = 0;
		}
		if (pps > max_bpm)
		{
			max_bpm = pps;
		}
	}
}

/*
	Peer
*/

PeerHelper::PeerHelper() :
	m_peer(0)
{
}

PeerHelper::PeerHelper(Peer* peer) :
	m_peer(peer)
{
	if (peer != NULL)
	{
		if (!peer->IncUseCount())
		{
			m_peer = 0;
		}
	}
}

PeerHelper::~PeerHelper()
{
	if (m_peer != 0)
		m_peer->DecUseCount();

	m_peer = 0;
}

PeerHelper& PeerHelper::operator=(Peer* peer)
{
	m_peer = peer;
	if (peer != NULL)
	{
		if (!peer->IncUseCount())
		{
			m_peer = 0;
		}
	}
	return *this;
}

Peer* PeerHelper::operator->() const
{
	return m_peer;
}

bool PeerHelper::operator!() {
	return ! m_peer;
}

bool PeerHelper::operator!=(void* ptr)
{
	return ((void*) m_peer != ptr);
}

Peer::Peer(u16 a_id, Address a_address):
	address(a_address),
	id(a_id),
	timeout_counter(0.0),
	ping_timer(0.0),
	resend_timeout(0.5),
	avg_rtt(-1.0),
	has_sent_with_id(false),
	m_sendtime_accu(0),
	m_num_sent(0),
	m_max_num_sent(0),
	congestion_control_aim_rtt(0.2),
	congestion_control_max_rate(400),
	congestion_control_min_rate(10),
	m_usage(0),
	m_pending_deletion(false)
{
}
Peer::~Peer()
{
	JMutexAutoLock usage_lock(m_exclusive_access_mutex);
	assert(m_usage == 0);
}

void Peer::reportRTT(float rtt)
{
	if(rtt < -0.999)
	{}
	else if(avg_rtt < 0.0)
		avg_rtt = rtt;
	else
		avg_rtt = rtt * 0.1 + avg_rtt * 0.9;
	
	float timeout = avg_rtt * RESEND_TIMEOUT_FACTOR;
	if(timeout < RESEND_TIMEOUT_MIN)
		timeout = RESEND_TIMEOUT_MIN;
	if(timeout > RESEND_TIMEOUT_MAX)
		timeout = RESEND_TIMEOUT_MAX;
	resend_timeout = timeout;
}

bool Peer::Ping(float dtime,SharedBuffer<u8>& data)
{
	ping_timer += dtime;
	if(ping_timer >= 5.0)
	{
		// Create and send PING packet
		writeU8(&data[0], TYPE_CONTROL);
		writeU8(&data[1], CONTROLTYPE_PING);
		ping_timer = 0.0;
		return true;
	}
	return false;
}

bool Peer::IncUseCount()
{
	JMutexAutoLock lock(m_exclusive_access_mutex);

	if (!m_pending_deletion)
	{
		this->m_usage++;
		return true;
	}

	return false;
}

void Peer::DecUseCount()
{
	{
		JMutexAutoLock lock(m_exclusive_access_mutex);
		assert(m_usage > 0);
		m_usage--;

		if (!((m_pending_deletion) && (m_usage == 0)))
			return;
	}
	delete this;
}

void Peer::Drop()
{
	{
		JMutexAutoLock usage_lock(m_exclusive_access_mutex);
		m_pending_deletion = true;
		if (m_usage != 0)
			return;
	}

	delete this;
}

void Peer::PutReliableSendCommand(ConnectionCommand &c,Connection* connection,unsigned int max_packet_size)
{
	JMutexAutoLock channel_lock(channels[c.channelnum].m_channel_mutex);

	if (channels[c.channelnum].outgoing_reliables_sent.size()
			< channels[c.channelnum].getWindowSize())
	{
		LOG(dout_con<<connection->getDesc()
				<<" processing reliable command for peer id: " << c.peer_id
				<<" data size: " << c.data.getSize() << std::endl);
		processReliableSendCommand(c,connection,max_packet_size);
	}
	else
	{
		LOG(dout_con<<connection->getDesc()
				<<" Queueing reliable command for peer id: " << c.peer_id
				<<" data size: " << c.data.getSize() <<std::endl);
		channels[c.channelnum].queued_commands.push_back(c);
	}
}

void Peer::processReliableSendCommand(ConnectionCommand &c,
				Connection* connection,
				unsigned int max_packet_size)
{
	u32 chunksize_max = max_packet_size
							- BASE_HEADER_SIZE
							- RELIABLE_HEADER_SIZE;

	std::list<SharedBuffer<u8> > originals;
	originals = makeAutoSplitPacket(c.data, chunksize_max,
			channels[c.channelnum].next_outgoing_split_seqnum);

	for(std::list<SharedBuffer<u8> >::iterator i = originals.begin();
		i != originals.end(); ++i)
	{
		u16 seqnum = channels[c.channelnum].getSequenceNumber();

		SharedBuffer<u8> reliable = makeReliablePacket(*i, seqnum);

		// Add base headers and make a packet
		BufferedPacket p = con::makePacket(address, reliable,
				connection->GetProtocolID(), connection->GetPeerID(),
				c.channelnum);

		channels[c.channelnum].queued_reliables.push_back(p);
	}
}

void Peer::RunCommandQueues(Connection* connection,
							unsigned int max_packet_size)
{

	for (unsigned int i = 0; i < CHANNEL_COUNT; i++)
	{
		JMutexAutoLock channel_lock(channels[i].m_channel_mutex);

		while ((channels[i].outgoing_reliables_sent.size()
					< channels[i].getWindowSize()) &&
				channels[i].queued_commands.size() > 0)
		{
			try {
				ConnectionCommand c = channels[i].queued_commands.pop_front();
				LOG(dout_con<<connection->getDesc()
						<<" processing queued reliable command "<<std::endl);
				processReliableSendCommand(c,connection,max_packet_size);
			}
			catch (ItemNotFoundException e) {
				// intentionaly empty
			}
		}
	}
}

/******************************************************************************/
/* Connection Threads                                                         */
/******************************************************************************/

#define CALC_DTIME(lasttime,curtime)                                           \
		MYMAX(MYMIN(((float) ( curtime - lasttime) / 1000),0.1),0.0)

ConnectionSendThread::ConnectionSendThread(Connection* parent,
											unsigned int max_packet_size,
											float timeout) :
	m_connection(parent),
	m_max_packet_size(max_packet_size),
	m_timeout(timeout)
{
}

void * ConnectionSendThread::Thread()
{
	ThreadStarted();
	log_register_thread("ConnectionSend");

	LOG(dout_con<<m_connection->getDesc()
			<<"ConnectionSend thread started"<<std::endl);

	u32 curtime = porting::getTimeMs();
	u32 lasttime = curtime;


	while(!StopRequested()) {
		BEGIN_DEBUG_EXCEPTION_HANDLER

		/* wait for trigger or timeout */
		m_send_sleep_semaphore.Wait(50);

		/* remove all triggers */
		while(m_send_sleep_semaphore.Wait(0)) {}

		lasttime = curtime;
		curtime = porting::getTimeMs();
		float dtime = CALC_DTIME(lasttime,curtime);

		/* first do all the reliable stuff */
		runTimeouts(dtime);

		/* translate commands to packets */
		ConnectionCommand c = m_connection->m_command_queue.pop_frontNE(0);
		while(c.type != CONNCMD_NONE){
			if (c.reliable)
				processReliableCommand(c);
			else
				processNonReliableCommand(c);
			c = m_connection->m_command_queue.pop_frontNE(0);
		}

		/* send non reliable packets */
		sendPackets(dtime);

		END_DEBUG_EXCEPTION_HANDLER(derr_con);
	}

	return NULL;
}

void ConnectionSendThread::Trigger()
{
	m_send_sleep_semaphore.Post();
}

void ConnectionSendThread::runTimeouts(float dtime)
{
	float congestion_control_aim_rtt
			= g_settings->getFloat("congestion_control_aim_rtt");
	float congestion_control_max_rate
			= g_settings->getFloat("congestion_control_max_rate");
	float congestion_control_min_rate
			= g_settings->getFloat("congestion_control_min_rate");

	std::list<u16> timeouted_peers;
	std::list<u16> peerIds = m_connection->getPeerIDs();

	for(std::list<u16>::iterator j = peerIds.begin();
		j != peerIds.end(); ++j)
	{
		PeerHelper peer = m_connection->getPeer(*j);

		if (!peer)
			continue;

		SharedBuffer<u8> data(2); // data for sending ping, required here because of goto

		// Update congestion control values
		peer->congestion_control_aim_rtt = congestion_control_aim_rtt;
		peer->congestion_control_max_rate = congestion_control_max_rate;
		peer->congestion_control_min_rate = congestion_control_min_rate;

		/*
			Check peer timeout
		*/
		peer->timeout_counter += dtime;
		if(peer->timeout_counter > m_timeout)
		{
			LOG(derr_con<<m_connection->getDesc()
					<<"RunTimeouts(): Peer "<<peer->id
					<<" has timed out."
					<<" (source=peer->timeout_counter)"
					<<std::endl);
			// Add peer to the list
			timeouted_peers.push_back(peer->id);
			// Don't bother going through the buffers of this one
			continue;
		}

		float resend_timeout = peer->resend_timeout;
		for(u16 i=0; i<CHANNEL_COUNT; i++)
		{
			std::list<BufferedPacket> timed_outs;
			Channel *channel = &peer->channels[i];
			JMutexAutoLock channelmutex(channel->m_channel_mutex);

			// Remove timed out incomplete unreliable split packets
			channel->incoming_splits.removeUnreliableTimedOuts(dtime, m_timeout);

			// Increment reliable packet times
			channel->outgoing_reliables_sent.incrementTimeouts(dtime);

			// Check reliable packet total times, remove peer if
			// over timeout.
			if(channel->outgoing_reliables_sent.anyTotaltimeReached(m_timeout))
			{
				LOG(derr_con<<m_connection->getDesc()
						<<"RunTimeouts(): Peer "<<peer->id
						<<" has timed out."
						<<" (source=reliable packet totaltime)"
						<<std::endl);
				// Add peer to the to-be-removed list
				timeouted_peers.push_back(peer->id);
				goto nextpeer;
			}

			// Re-send timed out outgoing reliables
			timed_outs = channel->
					outgoing_reliables_sent.getTimedOuts(resend_timeout);

			channel->UpdatePacketLossCounter(timed_outs.size());

			channel->outgoing_reliables_sent.resetTimedOuts(resend_timeout);

			for(std::list<BufferedPacket>::iterator j = timed_outs.begin();
				j != timed_outs.end(); ++j)
			{
				u16 peer_id = readPeerId(*(j->data));
				u8 channel = readChannel(*(j->data));
				u16 seqnum = readU16(&(j->data[BASE_HEADER_SIZE+1]));

				LOG(derr_con<<m_connection->getDesc()
						<<"RE-SENDING timed-out RELIABLE to "
						<< j->address.serializeString()
						<< "(t/o="<<resend_timeout<<"): "
						<<"from_peer_id="<<peer_id
						<<", channel="<<((int)channel&0xff)
						<<", seqnum="<<seqnum
						<<std::endl);

				rawSend(*j);

				// Enlarge avg_rtt and resend_timeout:
				// The rtt will be at least the timeout.
				// NOTE: This won't affect the timeout of the next
				// checked channel because it was cached.
				peer->reportRTT(resend_timeout);
			}
			channel->UpdateTimers(dtime);
		}

		/* send ping if necessary */
		if (peer->Ping(dtime,data)) {
			LOG(dout_con<<m_connection->getDesc()
					<<"Sending ping for peer_id: " << peer->id <<std::endl);
			rawSendAsPacket(peer->id, 0, data, true);
		}

		peer->RunCommandQueues(m_connection,m_max_packet_size);

nextpeer:
		continue;
	}

	// Remove timed out peers
	for(std::list<u16>::iterator i = timeouted_peers.begin();
		i != timeouted_peers.end(); ++i)
	{
		LOG(derr_con<<m_connection->getDesc()
				<<"RunTimeouts(): Removing peer "<<(*i)<<std::endl);
		m_connection->deletePeer(*i, true);
	}
}

void ConnectionSendThread::rawSend(const BufferedPacket &packet)
{
	try{
		m_connection->m_socket.Send(packet.address, *packet.data, packet.data.getSize());
		LOG(dout_con <<m_connection->getDesc()
				<< "rawSend: " << packet.data.getSize() << " bytes sent" << std::endl);
	} catch(SendFailedException &e){
		LOG(derr_con<<m_connection->getDesc()
				<<"Connection::rawSend(): SendFailedException: "
				<<packet.address.serializeString()<<std::endl);
	}
}

void ConnectionSendThread::sendAsPacketReliable(BufferedPacket& p, Channel* channel)
{
	try{
		// Buffer the packet
		channel->outgoing_reliables_sent.insert(p);
	}
	catch(AlreadyExistsException &e)
	{
		LOG(derr_con<<m_connection->getDesc()
				<<"WARNING: Going to send a reliable packet"
				<<" in outgoing buffer" <<std::endl);
		//assert(0);
	}

	// Send the packet
	rawSend(p);
}

bool ConnectionSendThread::rawSendAsPacket(u16 peer_id, u8 channelnum,
		SharedBuffer<u8> data, bool reliable)
{
	PeerHelper peer = m_connection->getPeerNoEx(peer_id);
	if(!peer) {
		LOG(dout_con<<m_connection->getDesc()
				<<"INFO: dropped packet for non existent peer_id: " << peer_id << std::endl);
		assert(reliable && "trying to send raw packet reliable but no peer found!");
		return false;
	}
	Channel *channel = &(peer->channels[channelnum]);

	if(reliable)
	{
		JMutexAutoLock channelmutex(channel->m_channel_mutex);


		u16 seqnum = channel->getSequenceNumber();

		SharedBuffer<u8> reliable = makeReliablePacket(data, seqnum);

		// Add base headers and make a packet
		BufferedPacket p = con::makePacket(peer->address, reliable,
				m_connection->GetProtocolID(), m_connection->GetPeerID(),
				channelnum);

		// first check if our send window is already maxed out
		if (channel->outgoing_reliables_sent.size()
				< channel->getWindowSize()) {
			LOG(dout_con<<m_connection->getDesc()
					<<"INFO: sending a reliable packet "
					<<" channel: " << channelnum
					<<" seqnum: " << seqnum << std::endl);
			sendAsPacketReliable(p,channel);
			return true;
		}
		else {
			LOG(dout_con<<m_connection->getDesc()
					<<"INFO: queued reliable packet for peer_id: " << peer_id << std::endl);
			channel->queued_reliables.push_back(p);
			return false;
		}
	}
	else
	{
		// Add base headers and make a packet
		BufferedPacket p = con::makePacket(peer->address, data,
				m_connection->GetProtocolID(), m_connection->GetPeerID(),
				channelnum);

		// Send the packet
		rawSend(p);
		return true;
	}

	//never reached
	return false;
}

void ConnectionSendThread::processReliableCommand(ConnectionCommand &c)
{
	assert(c.reliable);

	switch(c.type){
	case CONNCMD_NONE:
		LOG(dout_con<<m_connection->getDesc()<<" processing reliable CONNCMD_NONE"<<std::endl);
		return;

	case CONNCMD_SEND:
		LOG(dout_con<<m_connection->getDesc()<<" processing reliable CONNCMD_SEND"<<std::endl);
		sendReliable(c);
		return;

	case CONNCMD_SEND_TO_ALL:
		LOG(dout_con<<m_connection->getDesc()<<" processing CONNCMD_SEND_TO_ALL"<<std::endl);
		sendToAllReliable(c);
		return;

	case CONCMD_CREATE_PEER:
		LOG(dout_con<<m_connection->getDesc()<<" processing reliable CONCMD_CREATE_PEER"<<std::endl);
		rawSendAsPacket(c.peer_id,c.channelnum,c.data,c.reliable);
		return;

	case CONNCMD_SERVE:
	case CONNCMD_CONNECT:
	case CONNCMD_DISCONNECT:
	case CONNCMD_DELETE_PEER:
	case CONCMD_ACK:
		assert("Got command that shouldn't be reliable as reliable command" == 0);
	default:
		LOG(dout_con<<m_connection->getDesc()<<" Invalid reliable command type: " << c.type <<std::endl);
	}
}


void ConnectionSendThread::processNonReliableCommand(ConnectionCommand &c)
{
	assert(!c.reliable);

	switch(c.type){
	case CONNCMD_NONE:
		LOG(dout_con<<m_connection->getDesc()<<" processing CONNCMD_NONE"<<std::endl);
		return;
	case CONNCMD_SERVE:
		LOG(dout_con<<m_connection->getDesc()<<" processing CONNCMD_SERVE port="
				<<c.port<<std::endl);
		serve(c.port);
		return;
	case CONNCMD_CONNECT:
		LOG(dout_con<<m_connection->getDesc()<<" processing CONNCMD_CONNECT"<<std::endl);
		connect(c.address);
		return;
	case CONNCMD_DISCONNECT:
		LOG(dout_con<<m_connection->getDesc()<<" processing CONNCMD_DISCONNECT"<<std::endl);
		disconnect();
		return;
	case CONNCMD_SEND:
		LOG(dout_con<<m_connection->getDesc()<<" processing CONNCMD_SEND"<<std::endl);
		send(c.peer_id, c.channelnum, c.data);
		return;
	case CONNCMD_SEND_TO_ALL:
		LOG(dout_con<<m_connection->getDesc()<<" processing CONNCMD_SEND_TO_ALL"<<std::endl);
		sendToAll(c.channelnum, c.data);
		return;
	case CONNCMD_DELETE_PEER:
		LOG(dout_con<<m_connection->getDesc()<<" processing CONNCMD_DELETE_PEER"<<std::endl);
		m_connection->deletePeer(c.peer_id, false);
		return;
	case CONCMD_ACK:
		LOG(dout_con<<m_connection->getDesc()<<" processing CONCMD_ACK"<<std::endl);
		sendAsPacket(c.peer_id,c.channelnum,c.data,true);
		return;
	case CONCMD_CREATE_PEER:
		assert("Got command that should be reliable as unreliable command" == 0);
	default:
		LOG(dout_con<<m_connection->getDesc()<<" Invalid command type: " << c.type <<std::endl);
	}
}

void ConnectionSendThread::serve(u16 port)
{
	LOG(dout_con<<m_connection->getDesc()<<" serving at port "<<port<<std::endl);
	try{
		m_connection->m_socket.Bind(port);
		m_connection->SetPeerID(PEER_ID_SERVER);
	}
	catch(SocketException &e){
		// Create event
		ConnectionEvent ce;
		ce.bindFailed();
		m_connection->putEvent(ce);
	}
}

void ConnectionSendThread::connect(Address address)
{
	LOG(dout_con<<m_connection->getDesc()<<" connecting to "<<address.serializeString()
			<<":"<<address.getPort()<<std::endl);

	Peer *peer = m_connection->createServerPeer(address);

	// Create event
	ConnectionEvent e;
	e.peerAdded(peer->id, peer->address);
	m_connection->putEvent(e);

	m_connection->m_socket.Bind(0);

	// Send a dummy packet to server with peer_id = PEER_ID_INEXISTENT
	m_connection->SetPeerID(PEER_ID_INEXISTENT);
	SharedBuffer<u8> data(0);
	m_connection->Send(PEER_ID_SERVER, 0, data, true);
}

void ConnectionSendThread::disconnect()
{
	LOG(dout_con<<m_connection->getDesc()<<" disconnecting"<<std::endl);

	// Create and send DISCO packet
	SharedBuffer<u8> data(2);
	writeU8(&data[0], TYPE_CONTROL);
	writeU8(&data[1], CONTROLTYPE_DISCO);

	// Send to all
	sendToAll(0,data);
}

void ConnectionSendThread::send(u16 peer_id, u8 channelnum,
		SharedBuffer<u8> data)
{
	assert(channelnum < CHANNEL_COUNT);

	PeerHelper peer = m_connection->getPeerNoEx(peer_id);
	if(!peer) {
		LOG(dout_con<<m_connection->getDesc()<<" peer: peer_id="<<peer_id
				<< ">>>NOT<<< found on sending packet"
				<< ", channel " << (channelnum % 0xFF)
				<< ", size: " << data.getSize() <<std::endl);
		return;
	}

	LOG(dout_con<<m_connection->getDesc()<<" sending to peer_id="<<peer_id
			<< ", channel " << (channelnum % 0xFF)
			<< ", size: " << data.getSize() <<std::endl);

	Channel *channel = &(peer->channels[channelnum]);
	JMutexAutoLock channelmutex(channel->m_channel_mutex);

	u32 chunksize_max = m_max_packet_size - BASE_HEADER_SIZE;

	std::list<SharedBuffer<u8> > originals;
	originals = makeAutoSplitPacket(data, chunksize_max,
			channel->next_outgoing_split_seqnum);

	for(std::list<SharedBuffer<u8> >::iterator i = originals.begin();
		i != originals.end(); ++i)
	{
		SharedBuffer<u8> original = *i;
		sendAsPacket(peer_id, channelnum, original);
	}
}

void ConnectionSendThread::sendReliable(ConnectionCommand &c)
{
	PeerHelper peer = m_connection->getPeer(c.peer_id);
	peer->PutReliableSendCommand(c,m_connection,m_max_packet_size);
}

void ConnectionSendThread::sendToAll(u8 channelnum, SharedBuffer<u8> data)
{
	std::list<u16> peerids = m_connection->getPeerIDs();

	for (std::list<u16>::iterator i = peerids.begin();
			i != peerids.end();
			i++)
	{
		send(*i, channelnum, data);
	}
}

void ConnectionSendThread::sendToAllReliable(ConnectionCommand &c)
{
	std::list<u16> peerids = m_connection->getPeerIDs();

	for (std::list<u16>::iterator i = peerids.begin();
			i != peerids.end();
			i++)
	{
		PeerHelper peer = m_connection->getPeer(*i);
		peer->PutReliableSendCommand(c,m_connection,m_max_packet_size);
	}
}

void ConnectionSendThread::sendPackets(float dtime)
{
	std::list<u16> peerIds = m_connection->getPeerIDs();

	for(std::list<u16>::iterator
			j = peerIds.begin();
			j != peerIds.end(); ++j)
	{
		PeerHelper peer = m_connection->getPeer(*j);

		//peer may have been removed
		if (!peer) {
			LOG(dout_con<<m_connection->getDesc()<< " Peer not found: peer_id=" << *j << std::endl);
			continue;
		}

		peer->m_sendtime_accu += dtime;
		peer->m_num_sent = 0;
		peer->m_max_num_sent = peer->m_sendtime_accu *
				peer->m_max_packets_per_second;

		LOG(dout_con<<m_connection->getDesc()
				<< " Handle per peer queues: peer_id=" << *j << std::endl);
		// first send queued reliable packets for all peers (if possible)
		for (unsigned int i=0; i < CHANNEL_COUNT; i++)
		{
			JMutexAutoLock channellock(peer->channels[i].m_channel_mutex);
			LOG(dout_con<<m_connection->getDesc()<< "\t channel " << i
					<< std::endl << "\t\t\treliables on wire: " << peer->channels[i].outgoing_reliables_sent.size()
					<< std::endl << "\t\t\treliables queued : " << peer->channels[i].queued_reliables.size()
					<< std::endl << "\t\t\tqueued commands  : " << peer->channels[i].queued_commands.size()
					<< std::endl);

			while ((peer->channels[i].queued_reliables.size() > 0) &&
					(peer->channels[i].outgoing_reliables_sent.size()
							< peer->channels[i].getWindowSize()))
			{
				BufferedPacket p = peer->channels[i].queued_reliables.pop_front();
				Channel* channel = &(peer->channels[i]);
				LOG(dout_con<<m_connection->getDesc()
						<<"INFO: sending a queued reliable packet "
						<<" channel: " << i << std::endl);
				sendAsPacketReliable(p,channel);
			}
		}
	}

	/* send non reliable packets*/
	for(unsigned int i=0;i < m_outgoing_queue.size();i++) {
		OutgoingPacket packet = m_outgoing_queue.pop_front();
		assert(!packet.reliable && "a reliable packet somehow got to outgoing queue, this is not expected to happen");
		PeerHelper peer = m_connection->getPeerNoEx(packet.peer_id);
		if(!peer) {
			LOG(dout_con<<m_connection->getDesc()<<" Outgoing queue: peer_id="<<packet.peer_id
							<< ">>>NOT<<< found on sending packet"
							<< ", channel " << (packet.channelnum % 0xFF)
							<< ", reliable " << (packet.reliable ? "yes" : "no")
							<< ", size: " << packet.data.getSize() <<std::endl);
			continue;
		}
		/* send acks immediately */
		else if (packet.ack)
		{
			rawSendAsPacket(packet.peer_id, packet.channelnum,
								packet.data, packet.reliable);
		}
		else if(peer->m_num_sent < peer->m_max_num_sent){
			if (rawSendAsPacket(packet.peer_id, packet.channelnum,
					packet.data, packet.reliable))
			{
				peer->m_num_sent++;
			}
		}
		else {
			LOG(dout_con<<m_connection->getDesc()<<" Outgoing queue: peer_id="<<packet.peer_id
										<< " dropped for congestion reasons "
										<< ", channel " << (packet.channelnum % 0xFF)
										<< ", reliable " << (packet.reliable ? "yes" : "no")
										<< ", size: " << packet.data.getSize() <<std::endl);
		}
	}

	for(std::list<u16>::iterator
				j = peerIds.begin();
				j != peerIds.end(); ++j)
	{
		PeerHelper peer = m_connection->getPeerNoEx(*j);

		if(!peer)
			continue;

		if (peer->m_max_num_sent == 0)
			continue;

		peer->m_sendtime_accu -= (float)peer->m_num_sent /
				peer->m_max_packets_per_second;
		if(peer->m_sendtime_accu > 10. / peer->m_max_packets_per_second)
			peer->m_sendtime_accu = 10. / peer->m_max_packets_per_second;
	}
}

void ConnectionSendThread::sendAsPacket(u16 peer_id, u8 channelnum,
		SharedBuffer<u8> data, bool ack)
{
	OutgoingPacket packet(peer_id, channelnum, data, false, ack);
	m_outgoing_queue.push_back(packet);
}

ConnectionReceiveThread::ConnectionReceiveThread(Connection* parent,
												unsigned int max_packet_size) :
	m_connection(parent),
	m_max_packet_size(max_packet_size),
	m_indentation(0)
{
}

void * ConnectionReceiveThread::Thread()
{
	ThreadStarted();
	log_register_thread("ConnectionReceive");

	LOG(dout_con<<m_connection->getDesc()
			<<"ConnectionReceive thread started"<<std::endl);

	//u32 curtime = porting::getTimeMs();
	//u32 lasttime = curtime;

	while(!StopRequested()) {
		BEGIN_DEBUG_EXCEPTION_HANDLER

		//lasttime = curtime;
		//curtime = porting::getTimeMs();
		//float dtime = CALC_DTIME(lasttime,curtime);

		/* receive packets */
		receive();

		END_DEBUG_EXCEPTION_HANDLER(derr_con);
	}

	return NULL;
}

// Receive packets from the network and buffers and create ConnectionEvents
void ConnectionReceiveThread::receive()
{
	u32 datasize = m_max_packet_size * 2;  // Double it just to be safe
	// TODO: We can not know how many layers of header there are.
	// For now, just assume there are no other than the base headers.
	u32 packet_maxsize = datasize + BASE_HEADER_SIZE;
	SharedBuffer<u8> packetdata(packet_maxsize);

	bool single_wait_done = false;
	
	//TODO this loop is useless find a way to do it better
	for(u32 loop_i=0; loop_i<1000; loop_i++) // Limit in case of DoS
	{
	try{
		/* Check if some buffer has relevant data */
		{
			u16 peer_id;
			SharedBuffer<u8> resultdata;
			bool got = getFromBuffers(peer_id, resultdata);
			if(got){
				ConnectionEvent e;
				e.dataReceived(peer_id, resultdata);
				m_connection->putEvent(e);
				continue;
			}
		}
		
		if(single_wait_done){
			if(m_connection->m_socket.WaitData(0) == false)
				break;
		}
		
		single_wait_done = true;

		Address sender;
		s32 received_size = m_connection->m_socket.Receive(sender, *packetdata, packet_maxsize);

		if(received_size < 0)
			break;
		if(received_size < BASE_HEADER_SIZE)
			continue;
		if(readU32(&packetdata[0]) != m_connection->GetProtocolID())
			continue;
		
		u16 peer_id = readPeerId(*packetdata);
		u8 channelnum = readChannel(*packetdata);
		if(channelnum > CHANNEL_COUNT-1){
			LOG(derr_con<<m_connection->getDesc()
					<<"Receive(): Invalid channel "<<channelnum<<std::endl);
			throw InvalidIncomingDataException("Channel doesn't exist");
		}

		/*
			Try to identify peer by sender address
		 */
		if(peer_id == PEER_ID_INEXISTENT)
		{
			peer_id = m_connection->lookupPeer(sender);
		}
		
		/*
			The peer was not found in our lists. Add it.
		*/
		if(peer_id == PEER_ID_INEXISTENT)
		{
			peer_id = m_connection->createPeer(sender);
		}

		PeerHelper peer = m_connection->getPeerNoEx(peer_id);

		if (!peer) {
			LOG(dout_con<<m_connection->getDesc()
					<<"got packet from unknown peer_id: "<<peer_id<<" Ignoring."<<std::endl);
			continue;
		}

		// Validate peer address
		if(peer->address != sender)
		{
			LOG(derr_con<<m_connection->getDesc()
					<<m_connection->getDesc()
					<<"Peer "<<peer_id<<" sending from different address."
					" Ignoring."<<std::endl);
			continue;
		}
		
		LOG(dout_con<<m_connection->getDesc()
				<<"got packet from peer_id: "<<peer_id<<" resetting timeout"<<std::endl);
		peer->timeout_counter = 0.0;

		Channel *channel = &(peer->channels[channelnum]);
		JMutexAutoLock channelmutex(channel->m_channel_mutex);
		
		// Throw the received packet to channel->processPacket()

		// Make a new SharedBuffer from the data without the base headers
		SharedBuffer<u8> strippeddata(received_size - BASE_HEADER_SIZE);
		memcpy(*strippeddata, &packetdata[BASE_HEADER_SIZE],
				strippeddata.getSize());
		
		try{
			// Process it (the result is some data with no headers made by us)
			SharedBuffer<u8> resultdata = processPacket
					(channel, strippeddata, peer_id, channelnum, false);
			
			LOG(dout_con<<m_connection->getDesc()
					<<"ProcessPacket returned data of size "
					<<resultdata.getSize()<<std::endl);
			
			ConnectionEvent e;
			e.dataReceived(peer_id, resultdata);
			m_connection->putEvent(e);
			continue;
		}catch(ProcessedSilentlyException &e){
		}
	}catch(InvalidIncomingDataException &e){
	}
	catch(ProcessedSilentlyException &e){
	}
	} // for
}

bool ConnectionReceiveThread::getFromBuffers(u16 &peer_id, SharedBuffer<u8> &dst)
{
	std::list<u16> peerids = m_connection->getPeerIDs();

	for(std::list<u16>::iterator j = peerids.begin();
		j != peerids.end(); ++j)
	{
		PeerHelper peer = m_connection->getPeer(*j);
		if (!peer)
			continue;
		for(u16 i=0; i<CHANNEL_COUNT; i++)
		{
			Channel *channel = &peer->channels[i];
			JMutexAutoLock channellock(channel->m_channel_mutex);

			SharedBuffer<u8> resultdata;
			bool got = checkIncomingBuffers(channel, peer_id, resultdata);
			if(got){
				dst = resultdata;
				return true;
			}
		}
	}
	return false;
}

bool ConnectionReceiveThread::checkIncomingBuffers(Channel *channel, u16 &peer_id,
		SharedBuffer<u8> &dst)
{
	u16 firstseqnum = 0;
	// Clear old packets from start of buffer
	for(;;){
		bool found = channel->incoming_reliables.getFirstSeqnum(&firstseqnum);
		if(!found)
			break;
		if(seqnum_higher(channel->next_incoming_seqnum, firstseqnum))
			channel->incoming_reliables.popFirst();
		else
			break;
	}
	// This happens if all packets are old

	if(channel->incoming_reliables.empty() == false)
	{
		if(firstseqnum == channel->next_incoming_seqnum)
		{
			BufferedPacket p = channel->incoming_reliables.popFirst();

			peer_id = readPeerId(*p.data);
			u8 channelnum = readChannel(*p.data);
			u16 seqnum = readU16(&p.data[BASE_HEADER_SIZE+1]);

			LOG(dout_con<<m_connection->getDesc()
					<<"UNBUFFERING TYPE_RELIABLE"
					<<" seqnum="<<seqnum
					<<" peer_id="<<peer_id
					<<" channel="<<((int)channelnum&0xff)
					<<std::endl);

			channel->next_incoming_seqnum++;

			u32 headers_size = BASE_HEADER_SIZE + RELIABLE_HEADER_SIZE;
			// Get out the inside packet and re-process it
			SharedBuffer<u8> payload(p.data.getSize() - headers_size);
			memcpy(*payload, &p.data[headers_size], payload.getSize());

			dst = processPacket(channel, payload, peer_id, channelnum, true);
			return true;
		}
	}
	return false;
}

SharedBuffer<u8> ConnectionReceiveThread::processPacket(Channel *channel,
		SharedBuffer<u8> packetdata, u16 peer_id,
		u8 channelnum, bool reliable)
{
	IndentationRaiser iraiser(&(m_indentation));

	PeerHelper peer = m_connection->getPeer(peer_id);

	if(packetdata.getSize() < 1)
		throw InvalidIncomingDataException("packetdata.getSize() < 1");

	u8 type = readU8(&packetdata[0]);

	if(type == TYPE_CONTROL)
	{
		if(packetdata.getSize() < 2)
			throw InvalidIncomingDataException("packetdata.getSize() < 2");

		u8 controltype = readU8(&packetdata[1]);

		if(controltype == CONTROLTYPE_ACK)
		{
			if(packetdata.getSize() < 4)
				throw InvalidIncomingDataException
						("packetdata.getSize() < 4 (ACK header size)");

			u16 seqnum = readU16(&packetdata[2]);
			LOG(dout_con<<m_connection->getDesc()
					<<"Got CONTROLTYPE_ACK: channelnum="
					<<((int)channelnum&0xff)<<", peer_id="<<peer_id
					<<", seqnum="<<seqnum<<std::endl);

			try{
				BufferedPacket p = channel->outgoing_reliables_sent.popSeqnum(seqnum);
				// Get round trip time
				float rtt = p.totaltime;

				// Let peer calculate stuff according to it
				// (avg_rtt and resend_timeout)
				peer->reportRTT(rtt);

				//put bytes for max bandwidth calculation
				channel->UpdateBytesSent(p.data.getSize());
			}
			catch(NotFoundException &e){
				LOG(derr_con<<m_connection->getDesc()
						<<"WARNING: ACKed packet not "
						"in outgoing queue"
						<<std::endl);
			}

			throw ProcessedSilentlyException("Got an ACK");
		}
		else if(controltype == CONTROLTYPE_SET_PEER_ID)
		{
			// Got a packet to set our peer id
			if(packetdata.getSize() < 4)
				throw InvalidIncomingDataException
						("packetdata.getSize() < 4 (SET_PEER_ID header size)");
			u16 peer_id_new = readU16(&packetdata[2]);
			LOG(dout_con<<m_connection->getDesc()
					<<"Got new peer id: "<<peer_id_new<<"... "<<std::endl);

			if(m_connection->GetPeerID() != PEER_ID_INEXISTENT)
			{
				LOG(derr_con<<m_connection->getDesc()
						<<"WARNING: Not changing"
						" existing peer id."<<std::endl);
			}
			else
			{
				LOG(dout_con<<m_connection->getDesc()<<"changing own peer id"<<std::endl);
				m_connection->SetPeerID(peer_id_new);
			}
			throw ProcessedSilentlyException("Got a SET_PEER_ID");
		}
		else if(controltype == CONTROLTYPE_PING)
		{
			// Just ignore it, the incoming data already reset
			// the timeout counter
			LOG(dout_con<<m_connection->getDesc()<<"PING"<<std::endl);
			throw ProcessedSilentlyException("Got a PING");
		}
		else if(controltype == CONTROLTYPE_DISCO)
		{
			// Just ignore it, the incoming data already reset
			// the timeout counter
			LOG(dout_con<<m_connection->getDesc()
					<<"DISCO: Removing peer "<<(peer_id)<<std::endl);

			if(m_connection->deletePeer(peer_id, false) == false)
			{
				derr_con<<m_connection->getDesc()
						<<"DISCO: Peer not found"<<std::endl;
			}

			throw ProcessedSilentlyException("Got a DISCO");
		}
		else{
			LOG(derr_con<<m_connection->getDesc()
					<<"INVALID TYPE_CONTROL: invalid controltype="
					<<((int)controltype&0xff)<<std::endl);
			throw InvalidIncomingDataException("Invalid control type");
		}
	}
	else if(type == TYPE_ORIGINAL)
	{
		if(packetdata.getSize() < ORIGINAL_HEADER_SIZE)
			throw InvalidIncomingDataException
					("packetdata.getSize() < ORIGINAL_HEADER_SIZE");
		LOG(dout_con<<m_connection->getDesc()
				<<"RETURNING TYPE_ORIGINAL to user"
				<<std::endl);
		// Get the inside packet out and return it
		SharedBuffer<u8> payload(packetdata.getSize() - ORIGINAL_HEADER_SIZE);
		memcpy(*payload, &packetdata[ORIGINAL_HEADER_SIZE], payload.getSize());
		return payload;
	}
	else if(type == TYPE_SPLIT)
	{
		// We have to create a packet again for buffering
		// This isn't actually too bad an idea.
		BufferedPacket packet = makePacket(
				peer->address,
				packetdata,
				m_connection->GetProtocolID(),
				peer_id,
				channelnum);
		// Buffer the packet
		SharedBuffer<u8> data = channel->incoming_splits.insert(packet, reliable);
		if(data.getSize() != 0)
		{
			LOG(dout_con<<m_connection->getDesc()
					<<"RETURNING TYPE_SPLIT: Constructed full data, "
					<<"size="<<data.getSize()<<std::endl);
			return data;
		}
		LOG(dout_con<<m_connection->getDesc()<<"BUFFERED TYPE_SPLIT"<<std::endl);
		throw ProcessedSilentlyException("Buffered a split packet chunk");
	}
	else if(type == TYPE_RELIABLE)
	{
		// Recursive reliable packets not allowed
		if(reliable)
			throw InvalidIncomingDataException("Found nested reliable packets");

		if(packetdata.getSize() < RELIABLE_HEADER_SIZE)
			throw InvalidIncomingDataException
					("packetdata.getSize() < RELIABLE_HEADER_SIZE");

		u16 seqnum = readU16(&packetdata[1]);

		bool is_future_packet = seqnum_higher(seqnum, channel->next_incoming_seqnum);
		bool is_old_packet = seqnum_higher(channel->next_incoming_seqnum, seqnum);


		LOG(dout_con<<m_connection->getDesc()
				<<(is_future_packet ? "BUFFERING," : "")
				<<(is_old_packet ? "OLD," : "")
				<<((!is_future_packet && !is_old_packet) ? "RECURSIVE," : "")
				<<" TYPE_RELIABLE seqnum packet seqnum="<<seqnum
				<<" channel next seqnum="<<channel->next_incoming_seqnum
				<<" channel=" << (channelnum & 0xFF)
				<<" [sending CONTROLTYPE_ACK"
				<<" to peer_id="<<peer_id<<"]"
				<<std::endl);

		//DEBUG
		//assert(channel->incoming_reliables.size() < 100);
		m_connection->sendAck(peer_id,channelnum,seqnum);


		//if(seqnum_higher(seqnum, channel->next_incoming_seqnum))
		if(is_future_packet)
		{
			// This one comes later, buffer it.
			// Actually we have to make a packet to buffer one.
			// Well, we have all the ingredients, so just do it.
			BufferedPacket packet = con::makePacket(
					peer->address,
					packetdata,
					m_connection->GetProtocolID(),
					peer_id,
					channelnum);
			try{
				channel->incoming_reliables.insert(packet);
			}
			catch(AlreadyExistsException &e)
			{
			}

			throw ProcessedSilentlyException("Buffered future reliable packet");
		}
		//else if(seqnum_higher(channel->next_incoming_seqnum, seqnum))
		else if(is_old_packet)
		{
			// An old packet, dump it
			throw InvalidIncomingDataException("Got an old reliable packet");
		}

		channel->next_incoming_seqnum++;

		// Get out the inside packet and re-process it
		SharedBuffer<u8> payload(packetdata.getSize() - RELIABLE_HEADER_SIZE);
		memcpy(*payload, &packetdata[RELIABLE_HEADER_SIZE], payload.getSize());

		return processPacket(channel, payload, peer_id, channelnum, true);
	}
	else
	{
		derr_con<<m_connection->getDesc()
				<<"Got invalid type="<<((int)type&0xff)<<std::endl;
		throw InvalidIncomingDataException("Invalid packet type");
	}

	// We should never get here.
	// If you get here, add an exception or a return to some of the
	// above conditionals.
	assert(0);
	throw BaseException("Error in Channel::ProcessPacket()");
}

/*
	Connection
*/

Connection::Connection(u32 protocol_id, u32 max_packet_size, float timeout,
		bool ipv6):
	m_socket(ipv6),
	m_command_queue(),
	m_event_queue(),
	m_peer_id(0),
	m_protocol_id(protocol_id),
	m_sendThread(this, max_packet_size, timeout),
	m_receiveThread(this, max_packet_size),
	m_info_mutex(),
	m_bc_peerhandler(0),
	m_bc_receive_timeout(0)
{
	m_socket.setTimeoutMs(5);

	m_sendThread.Start();
	m_receiveThread.Start();
}

Connection::Connection(u32 protocol_id, u32 max_packet_size, float timeout,
		bool ipv6, PeerHandler *peerhandler):
	m_socket(ipv6),
	m_command_queue(),
	m_event_queue(),
	m_peer_id(0),
	m_protocol_id(protocol_id),
	m_sendThread(this, max_packet_size, timeout),
	m_receiveThread(this, max_packet_size),
	m_info_mutex(),
	m_bc_peerhandler(peerhandler),
	m_bc_receive_timeout(0)

{
	m_socket.setTimeoutMs(5);

	m_sendThread.Start();
	m_receiveThread.Start();
}


Connection::~Connection()
{
	// request threads to stop
	m_sendThread.Stop();
	m_receiveThread.Stop();

	// wait for threads to finish
	m_sendThread.Wait();
	m_receiveThread.Wait();

	// Delete peers
	for(std::map<u16, Peer*>::iterator
			j = m_peers.begin();
			j != m_peers.end(); ++j)
	{
		delete j->second;
	}
}

/* Internal stuff */
void Connection::putEvent(ConnectionEvent &e)
{
	assert(e.type != CONNEVENT_NONE);
	m_event_queue.push_back(e);
}

PeerHelper Connection::getPeer(u16 peer_id)
{
	JMutexAutoLock peerlock(m_peers_mutex);
	std::map<u16, Peer*>::iterator node = m_peers.find(peer_id);

	if(node == m_peers.end()){
		throw PeerNotFoundException("GetPeer: Peer not found (possible timeout)");
	}

	// Error checking
	assert(node->second->id == peer_id);

	return PeerHelper(node->second);
}

PeerHelper Connection::getPeerNoEx(u16 peer_id)
{
	JMutexAutoLock peerlock(m_peers_mutex);
	std::map<u16, Peer*>::iterator node = m_peers.find(peer_id);

	if(node == m_peers.end()){
		return NULL;
	}

	// Error checking
	assert(node->second->id == peer_id);

	return PeerHelper(node->second);
}

/* find peer_id for address */
u16 Connection::lookupPeer(Address& sender)
{
	JMutexAutoLock peerlock(m_peers_mutex);
	std::map<u16, Peer*>::iterator j;
	j = m_peers.begin();
	for(; j != m_peers.end(); ++j)
	{
		Peer *peer = j->second;
		if(peer->has_sent_with_id)
			continue;
		if(peer->address == sender)
			return peer->id;
	}

	return PEER_ID_INEXISTENT;
}

std::list<Peer*> Connection::getPeers()
{
	std::list<Peer*> list;
	for(std::map<u16, Peer*>::iterator j = m_peers.begin();
		j != m_peers.end(); ++j)
	{
		Peer *peer = j->second;
		list.push_back(peer);
	}
	return list;
}

bool Connection::deletePeer(u16 peer_id, bool timeout)
{
	Peer *peer = 0;

	/* lock list as short as possible */
	{
		JMutexAutoLock peerlock(m_peers_mutex);
		if(m_peers.find(peer_id) == m_peers.end())
			return false;
		peer = m_peers[peer_id];
		m_peers.erase(peer_id);
	}

	// Create event
	ConnectionEvent e;
	e.peerRemoved(peer_id, timeout, peer->address);
	putEvent(e);


	peer->Drop();
	return true;
}

/* Interface */

ConnectionEvent Connection::getEvent()
{
	if(m_event_queue.empty()){
		ConnectionEvent e;
		e.type = CONNEVENT_NONE;
		return e;
	}
	return m_event_queue.pop_front();
}

ConnectionEvent Connection::waitEvent(u32 timeout_ms)
{
	try{
		return m_event_queue.pop_front(timeout_ms);
	} catch(ItemNotFoundException &ex){
		ConnectionEvent e;
		e.type = CONNEVENT_NONE;
		return e;
	}
}

void Connection::putCommand(ConnectionCommand &c)
{
	m_command_queue.push_back(c);
}

void Connection::Serve(unsigned short port)
{
	ConnectionCommand c;
	c.serve(port);
	putCommand(c);
}

void Connection::Connect(Address address)
{
	ConnectionCommand c;
	c.connect(address);
	putCommand(c);
}

bool Connection::Connected()
{
	JMutexAutoLock peerlock(m_peers_mutex);

	if(m_peers.size() != 1)
		return false;
		
	std::map<u16, Peer*>::iterator node = m_peers.find(PEER_ID_SERVER);
	if(node == m_peers.end())
		return false;
	
	if(m_peer_id == PEER_ID_INEXISTENT)
		return false;
	
	return true;
}

void Connection::Disconnect()
{
	ConnectionCommand c;
	c.disconnect();
	putCommand(c);
}

u32 Connection::Receive(u16 &peer_id, SharedBuffer<u8> &data)
{
	for(;;){
		ConnectionEvent e = waitEvent(m_bc_receive_timeout);
		if(e.type != CONNEVENT_NONE)
			LOG(dout_con<<getDesc()<<": Receive: got event: "
					<<e.describe()<<std::endl);
		switch(e.type){
		case CONNEVENT_NONE:
			throw NoIncomingDataException("No incoming data");
		case CONNEVENT_DATA_RECEIVED:
			peer_id = e.peer_id;
			data = SharedBuffer<u8>(e.data);
			return e.data.getSize();
		case CONNEVENT_PEER_ADDED: {
			Peer tmp(e.peer_id, e.address);
			if(m_bc_peerhandler)
				m_bc_peerhandler->peerAdded(&tmp);
			continue; }
		case CONNEVENT_PEER_REMOVED: {
			Peer tmp(e.peer_id, e.address);
			if(m_bc_peerhandler)
				m_bc_peerhandler->deletingPeer(&tmp, e.timeout);
			continue; }
		case CONNEVENT_BIND_FAILED:
			throw ConnectionBindFailed("Failed to bind socket "
					"(port already in use?)");
		}
	}
	throw NoIncomingDataException("No incoming data");
}

void Connection::SendToAll(u8 channelnum, SharedBuffer<u8> data, bool reliable)
{
	assert(channelnum < CHANNEL_COUNT);

	ConnectionCommand c;
	c.sendToAll(channelnum, data, reliable);
	putCommand(c);
	m_sendThread.Trigger();
}

void Connection::Send(u16 peer_id, u8 channelnum,
		SharedBuffer<u8> data, bool reliable)
{
	assert(channelnum < CHANNEL_COUNT);

	ConnectionCommand c;
	c.send(peer_id, channelnum, data, reliable);
	putCommand(c);
}

void Connection::RunTimeouts(float dtime)
{
	// No-op
}

Address Connection::GetPeerAddress(u16 peer_id)
{
	PeerHelper peer = getPeer(peer_id);
	assert(peer != 0);
	return peer->address;
}

float Connection::GetPeerAvgRTT(u16 peer_id)
{
	return getPeer(peer_id)->avg_rtt;
}

u16 Connection::createPeer(Address& sender)
{
	// Somebody wants to make a new connection

	// Get a unique peer id (2 or higher)
	u16 peer_id_new = 2;
	/*
		Find an unused peer id
	*/
	bool out_of_ids = false;
	for(;;)
	{
		// Check if exists
		if(m_peers.find(peer_id_new) == m_peers.end())
			break;
		// Check for overflow
		if(peer_id_new == 65535){
			out_of_ids = true;
			break;
		}
		peer_id_new++;
	}
	if(out_of_ids){
		errorstream<<getDesc()<<" ran out of peer ids"<<std::endl;
		return PEER_ID_INEXISTENT;
	}

	LOG(dout_con<<getDesc()
			<<"createPeer(): giving peer_id="<<peer_id_new<<std::endl);

	// Create a peer
	Peer *peer = new Peer(peer_id_new, sender);
	m_peers_mutex.Lock();
	m_peers[peer->id] = peer;
	m_peers_mutex.Unlock();

	// Create peer addition event
	ConnectionEvent e;
	e.peerAdded(peer_id_new, sender);
	putEvent(e);

	ConnectionCommand cmd;
	SharedBuffer<u8> reply(4);
	writeU8(&reply[0], TYPE_CONTROL);
	writeU8(&reply[1], CONTROLTYPE_SET_PEER_ID);
	writeU16(&reply[2], peer_id_new);
	cmd.createPeer(peer_id_new,reply);
	this->putCommand(cmd);

	// We're now talking to a valid peer_id
	return peer_id_new;
}

void Connection::DeletePeer(u16 peer_id)
{
	ConnectionCommand c;
	c.deletePeer(peer_id);
	putCommand(c);
}

void Connection::PrintInfo(std::ostream &out)
{
	m_info_mutex.Lock();
	out<<getDesc()<<": ";
	m_info_mutex.Unlock();
}

void Connection::PrintInfo()
{
	PrintInfo(dout_con);
}

const std::string Connection::getDesc()
{
	return std::string("con(")+itos(m_socket.GetHandle())+"/"+itos(m_peer_id)+")";
}

void Connection::sendAck(u16 peer_id, u8 channelnum, u16 seqnum) {

	assert(channelnum < CHANNEL_COUNT);

	LOG(dout_con<<getDesc()
			<<"Sending ACK for peer_id: " << peer_id <<
			" channel: " << (channelnum & 0xFF) <<
			" seqnum: " << seqnum << std::endl);

	ConnectionCommand c;
	SharedBuffer<u8> ack(4);
	writeU8(&ack[0], TYPE_CONTROL);
	writeU8(&ack[1], CONTROLTYPE_ACK);
	writeU16(&ack[2], seqnum);

	c.ack(peer_id, channelnum, ack);
	putCommand(c);
	m_sendThread.Trigger();
}

Peer* Connection::createServerPeer(Address& address)
{
	if (getPeerNoEx(PEER_ID_SERVER) != 0)
	{
		throw ConnectionException("Already connected to a server");
	}

	Peer *peer = new Peer(PEER_ID_SERVER, address);

	{
		JMutexAutoLock lock(m_peers_mutex);
		m_peers[peer->id] = peer;
	}

	return peer;
}

std::list<u16> Connection::getPeerIDs()
{
	std::list<u16> retval;

	JMutexAutoLock lock(m_peers_mutex);
	for(std::map<u16, Peer*>::iterator j = m_peers.begin();
		j != m_peers.end(); ++j)
	{
		retval.push_back(j->first);
	}
	return retval;
}


} // namespace

