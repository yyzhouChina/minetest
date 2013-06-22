dofile(minetest.get_modpath("__builtin").."/serialize.lua")
dofile(minetest.get_modpath("__builtin").."/misc_helpers.lua")

function async_thread_step()
	while(not minetest.stop_async_thread()) do
	
		local job = minetest.async_get_job()
		
		if job ~= nil then
			local dummy_fct = loadstring(job.tocall)
			local parameters = minetest.deserialize(job.parameters)
			
			local tocall = dummy_fct()
			
			local status,result = pcall(tocall,parameters)
	
			if not status then
				minetest.log("action","execution of async funtion failed")
			end
			minetest.async_push_result(job.id,minetest.serialize(result))
		end
		
		minetest.async_yield()
	end
end

async_thread_step()

minetest.log("action", "Async Thread finished")