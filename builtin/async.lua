minetest.register_async_function = function(worker_function,parameters)
	if worker_function == nil or
		parameters == nil then
		return false;
	end
	
	local new_element = {
		tocall = DataDumper(worker_function),
		parameters = minetest.serialize(parameters),
		}

	return minetest.schedule_job(new_element);
end

local async_callbacks = {}

async_on_step_handler = function()

	local result_table = minetest.get_job_results()
	
	if #result_table == 0 then
		return
	end	
	
	for i=1,#result_table,1 do
		local handled = false
		
		if async_callbacks then
			for j=1, #async_callbacks, 1 do
				if async_callbacks[j](
						result.table[i].id, 
						minetest.deserialize(result.table[i].result)) then
					handled = true
					break
				end
			end
		end
		
		if not handled then
			minetest.log("action","got job result for job: " .. 
				result_table[i].id .. " but no one cared")
		end
	end
end

minetest.register_async_callback_handler = function(callbackhandler)
	table.insert(async_callbacks,callbackhandler)
end