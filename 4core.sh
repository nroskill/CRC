make clean
make
./bin/CMPsim.usetrace.64 -threads 4 -mix ./traces/179_181_179_181.mix -cache UL3:4096:64:16 -autorewind 1 -icount 1 -LLCrepl 5
#./bin/CMPsim.usetrace.64 -threads 1 -t ./traces/181.out.trace.gz -cache UL3:1024:64:16 -icount 1 -LLCrepl 5