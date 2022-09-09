Please follow the instructions for various options


	1. Uncomment the example you want to run in example_mmf2_video_surport()
	Note: If user want to use the examples, please check the defuault example "mmf2_video_example_v1_init" is comment
	
	2. cd project/realtek_amebapro2_v0_example/GCC-RELEASE

	3. mkdir build

	4. cd build

	5. cmake .. -G"Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE=../toolchain.cmake -DVIDEO_EXAMPLE=on

	6. cmake --build . --target flash
 
