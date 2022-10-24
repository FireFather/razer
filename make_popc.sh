arch_cpu=x86-64-popc
make --no-print-directory -j profile-build ARCH=${arch_cpu} COMP=mingw
strip razer.exe
mv razer.exe Razer_0.0_x64.exe 
make clean 
