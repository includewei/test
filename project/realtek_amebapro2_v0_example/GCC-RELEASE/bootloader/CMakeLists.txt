cmake_minimum_required(VERSION 3.6)

project(bootloader)

# root of realtek_amebapro2_v0_example
set (prj_root "${CMAKE_CURRENT_SOURCE_DIR}/../..")
# root of SDK
set (sdk_root "${CMAKE_CURRENT_SOURCE_DIR}/../../../..")
set(bootloader bootloader)

#set(freertos freertos_v202012.00)

include(../config.cmake)

include(libboot.cmake OPTIONAL)

if(NOT BUILD_LIB)
	link_directories(${prj_root}/GCC-RELEASE/bootloader/output)
endif()


#VIDEO
list(
	APPEND bootloader_sources
	${sdk_root}/component/video/driver/RTL8735B/video_user_boot.c
	${sdk_root}/component/soc/8735b/misc/platform/user_boot.c
)

add_executable(
	${bootloader}
	${bootloader_sources}
)

list(
	APPEND boot_flags
	CONFIG_BUILD_BOOT=1
	CONFIG_BUILD_RAM=1 
	CONFIG_BUILD_LIB=1 
	CONFIG_PLATFORM_8735B
	CONFIG_RTL8735B_PLATFORM=1
)

target_compile_definitions(${bootloader} PRIVATE ${boot_flags} )

include(../includepath.cmake)
target_include_directories(
	${bootloader}
	PUBLIC

	${inc_path}
	${sdk_root}/component/os/freertos/${freertos}/Source/portable/GCC/ARM_CM33_NTZ/non_secure
	${sdk_root}/component/soc/8735b/fwlib/rtl8735b/lib/source/ram/video
	${sdk_root}/component/soc/8735b/fwlib/rtl8735b/lib/source/ram/video/enc/inc
)

target_link_libraries(
	${bootloader} 
	-Wl,--whole-archive
	boot
)

if(NOT PICOLIBC)
target_link_libraries(
	${bootloader} 
	nosys
)
endif()

if(MPCHIP)
	set(ld_script "${CMAKE_CURRENT_SOURCE_DIR}/rtl8735b_boot_mp.ld")
else()
	set(ld_script "${CMAKE_CURRENT_SOURCE_DIR}/rtl8735b_boot.ld")
endif()
target_link_options(
	${bootloader} 
	PUBLIC
	"LINKER:SHELL:-L ${CMAKE_CURRENT_SOURCE_DIR}/../ROM/GCC"
	"LINKER:SHELL:-T ${ld_script}"
	"LINKER:SHELL:-Map=${CMAKE_CURRENT_BINARY_DIR}/bootloader.map"
)

set_target_properties(${bootloader} PROPERTIES LINK_DEPENDS ${ld_script})
set_target_properties(${bootloader} PROPERTIES LINK_DEPENDS ${ELF2BIN})

if (MPCHIP)	

add_custom_command(TARGET ${bootloader} POST_BUILD 

	COMMAND ${CMAKE_NM} $<TARGET_FILE:${bootloader}> | sort > ${bootloader}.nm.map
	COMMAND ${CMAKE_OBJEDUMP} -d $<TARGET_FILE:${bootloader}> > ${bootloader}.asm
	COMMAND cp $<TARGET_FILE:${bootloader}> ${bootloader}.axf

	COMMAND cp ${POSTBUILD_BOOT} amebapro2_bootloader.json
	COMMAND ${ELF2BIN} convert amebapro2_bootloader.json BOOTLOADER boot.bin

	COMMAND rm -rf output && mkdir output
	COMMAND cp boot.bin output
	COMMAND cp ${bootloader}.nm.map output
	COMMAND cp ${bootloader}.asm output
	COMMAND cp ${bootloader}.axf output
)
else()
add_custom_command(TARGET ${bootloader} POST_BUILD 
	COMMAND ${CMAKE_NM} $<TARGET_FILE:${bootloader}> | sort > ${bootloader}.nm.map
	COMMAND ${CMAKE_OBJEDUMP} -d $<TARGET_FILE:${bootloader}> > ${bootloader}.asm
	COMMAND cp $<TARGET_FILE:${bootloader}> ${bootloader}.axf

	COMMAND cp ${prj_root}/GCC-RELEASE/keycfg.json keycfg.json
	COMMAND cp ${prj_root}/GCC-RELEASE/partition.json partition.json
	COMMAND cp ${prj_root}/GCC-RELEASE/amebapro2_bootloader.json amebapro2_bootloader.json
	COMMAND ${ELF2BIN} keygen keycfg.json
	COMMAND ${ELF2BIN} convert amebapro2_bootloader.json BOOTLOADER

	COMMAND rm -rf output && mkdir output
	COMMAND cp boot.bin output
	COMMAND cp ${bootloader}.nm.map output
	COMMAND cp ${bootloader}.asm output
	COMMAND cp ${bootloader}.axf output
)
endif()	