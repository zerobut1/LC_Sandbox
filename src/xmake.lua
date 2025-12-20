includes("test")
includes("Yutrel")

-- Enable CUDA device runtime (cudadevrt) embedding for LuisaCompute CUDA backend.
-- This removes the runtime warning:
--   "CUDA Device Runtime library is not linked. Indirect dispatch will not be available..."
-- We do it here (under src/) by patching the already-defined target from ext/LuisaCompute.
after_load(function()
	import("core.project.project")

	local cuda_backend = project.target("lc-backend-cuda")
	if cuda_backend == nil then
		return
	end

	local cuda_path = os.getenv("CUDA_PATH")
	if cuda_path == nil or cuda_path == "" then
		wprint("CUDA_PATH is not set; cannot locate cudadevrt.lib. CUDA indirect dispatch will remain unavailable.")
		return
	end

	local cudadevrt = path.join(cuda_path, "lib", "x64", "cudadevrt.lib")
	if not os.isfile(cudadevrt) then
		wprint("cudadevrt.lib not found at '%s'. CUDA indirect dispatch will remain unavailable.", cudadevrt)
		return
	end

	local gen_dir = path.join(os.projectdir(), "build", "generated", "luisa", "cuda_devrt")
	local gen_cpp = path.join(gen_dir, "cuda_devrt_embedded.cpp")
	os.mkdir(gen_dir)

	local function _generate_bin2c(input_file, output_cpp)
		local fin = io.open(input_file, "rb")
		if fin == nil then
			raise("failed to open file: " .. input_file)
		end
		local data = fin:read("*all")
		fin:close()

		local fout = io.open(output_cpp, "w")
		if fout == nil then
			raise("failed to write file: " .. output_cpp)
		end
		fout:write("// GENERATED FILE. DO NOT EDIT.\n")
		fout:write("extern \"C\" const unsigned char luisa_compute_cudadevrt[" .. tostring(#data) .. "] = {\n")
		for i = 1, #data do
			fout:write(tostring(data:byte(i)))
			if i ~= #data then
				fout:write(",")
			end
			if i % 32 == 0 then
				fout:write("\n")
			end
		end
		fout:write("\n};\n")
		fout:write("extern \"C\" const unsigned long long luisa_compute_cudadevrt_size = " .. tostring(#data) .. "ull;\n")
		fout:close()
	end

	local need_regen = (not os.isfile(gen_cpp)) or (os.mtime(gen_cpp) < os.mtime(cudadevrt))
	if need_regen then
		_generate_bin2c(cudadevrt, gen_cpp)
	end

	cuda_backend:add("defines", "LUISA_COMPUTE_ENABLE_CUDADEVRT=1")
	cuda_backend:add("files", gen_cpp)
end)