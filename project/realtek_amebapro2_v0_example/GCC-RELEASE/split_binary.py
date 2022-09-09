#!/usr/bin/env python3

import os
import json
import sys
import re
import json


#root_config_jason_name = '../../config_root.json'
#cwd = os.path.dirname(os.path.realpath(__file__))
#jsonfile = open(os.path.join(cwd,root_config_jason_name),'r')

#ocfg = json.load(jsonfile)
#jsonfile.close()


#if 'ASDK_Path' in ocfg:
	#asdk_path = ocfg['ASDK_Path']
	#OBJDUMP = asdk_path+"/bin/arm-none-eabi-objdump"
	#OBJCOPY = asdk_path+"/bin/arm-none-eabi-objcopy"
OBJDUMP = "arm-none-eabi-objdump"
OBJCOPY = "arm-none-eabi-objcopy"

bin_ntz = [
#		["itcm_rom",	0x00000000,32*1024		,'top']	
#		,["itcm_ram",	0x00010000,128*1024		,'top']
		["itcm_rom",	0x00000000,192*1024		,'top']		
		,["dtcm_rom",	0x20010000,80*1024		,'top']
		,["dtcm_ram",	0x20000000,16*1024		,'top']
		,["rom",		0x10000000,768*1024		,'top']
		,["ram",		0x20100000,512*1024		,'top']
		,["ddr",		0x70000000,44*1024*1024	,'top']
		]

bin_s = [
		 ["itcm_rom_s",	0x00000000,12*1024		,'top']	
		,["dtcm_rom_s",	0x20010000,30*1024		,'top']
		,["dtcm_ram_s",	0x20002000,8*1024		,'bottom']
		,["rom_s",		0x10000000,298*1024		,'top']
		,["ram_nsc",	0x20160000,24*1024		,'middle']
		,["ram_s",		0x20166000,104*1024		,'bottom']
		,["ddr_s",		0x70000000,2*1024*1024	,'top']
		]

bin_ns = [
		 ["itcm_rom_ns",0x00003000,20*1024		,'bottom']	
		,["itcm_ram_ns",0x00010000,128*1024		,'top']
		,["dtcm_rom_ns",0x20017800,50*1024		,'bottom']
		,["dtcm_ram_ns",0x20000000,8*1024		,'top']
		,["rom_ns",		0x1004A800,470*1024		,'bottom']
		,["ram_ns",		0x20100000,384*1024		,'top']
		,["ddr_ns",		0x70200000,62*1024*1024	,'bottom']
		]

merge_tz = [	
		 ["itcm_rom",	"itcm_rom_s",	"itcm_rom_ns"]		
		,["dtcm_rom",	"dtcm_rom_s",	"dtcm_rom_ns"]
		,["dtcm_ram",	"dtcm_ram_ns",	"dtcm_ram_s"]
		,["rom",		"rom_s",		"rom_ns"]
		,["ram",		"ram_ns",		"ram_nsc",	"ram_s"]
		,["ddr",		"ddr_s",		"ddr_ns"]
		]

def main():
	print("=========binary gen.============")
	
	if len(sys.argv) < 3:
		print("split binary command fail")
		exit()
		
	output_dir = sys.argv[1]
	type = sys.argv[2]
	section_info = output_dir+'/section_info.txt'
	
	if type == "NonSecure":
		binary = bin_ns
		axf = output_dir+'/application.ns.axf'
	elif type == "Secure":
		binary = bin_s
		axf = output_dir+'/application.s.axf'			
	else:
		binary = bin_ntz
		axf = output_dir+'/application.axf'
		
	cmd = OBJDUMP+" -h "+axf+" > "+section_info
	#print(cmd)
	os.system(cmd)
	
	file = open(section_info, "r")
	line = file.readline()

		
	while line:	 
		if (len(line.split()) > 0) and (line.split()[0].isdigit()):			
			line2 = file.readline()
			#if line2.find("LOAD") != -1:
			addr = int(line.split()[3], 16)
			section = line.split()[1]
			for region in binary:
				# arm-obj-copy can't find .data_of_rom section ???????
				# workaround remove .data_of_rom							
				if (addr >= region[1] and addr <= (region[1]+region[2])) and (section != ".data_of_rom"):
					
					#print(hex(addr), section)
					region.append(section)
								
		line = file.readline()		
	file.close()

	cmd = "dd if=/dev/zero of="+output_dir+"/ddr_calibration.bin bs=1 count=64"
	#print(cmd)
	os.system(cmd)
	
	bin2hex = "python "+os.getcwd()+"/../utility/post_build/BinToHexdump.py "
	align_file = output_dir+"/align.bin"
	
	for region in binary:
		output_bin = output_dir+"/"+region[0]+".bin "
		output_hex = output_dir+"/"+region[0]+".hex "
		# objcopy command	
		if len(region) >4:
			#print(region)
	
			cmd = OBJCOPY
			for i in range(4, len(region)):
				cmd += " -j "+region[i]
			cmd +=" -Obinary "+axf+" "+ output_bin
						
			if (region[0].find("ddr") != -1) and (region[3].find("top") != -1):
				cmd += "; cat "+output_dir+"/"+region[0]+".bin >> "+output_dir+"/ddr_calibration.bin"
				cmd += "; mv "+output_dir+"/ddr_calibration.bin "+output_dir+"/"+region[0]+".bin"
				
			#print(cmd)		
			os.system(cmd)
			
			file_size = os.path.getsize(output_dir+"/"+region[0]+".bin")
		
			align_size = int(hex((file_size+31) & 0xffffffe0),16)			
			cmd ="truncate -s "+str(align_size)+" "+output_bin+";"


			if type == "Ignore":
				if region[0].find("ddr") != -1:
					cmd += "hexdump -v -e '2/1 \"%02x \"' -e '\"\\n\"' "
					cmd += output_bin+" | awk '{ print $8$7$6$5$4$3$2$1 }'  > "+output_hex+";"				
				else:
					cmd += "hexdump -v -e '8/1 \"%02x \"' -e '\"\\n\"' "
					cmd += output_bin+" | awk '{ print $8$7$6$5$4$3$2$1 }'  > "+output_hex+";"

			#print(cmd)						
			os.system(cmd)
		
		# Add padding on top/middle section	and NonSecure/Secure	
		if (((region[3].find("bottom") == -1) and (type != "Ignore")) or (region[0].find("rom") != -1)):		
		#if ((region[3].find("bottom") == -1) and (type != "Ignore")):			
			# Added padding
			cmd = "truncate -s "+str(region[2])+" "+output_bin
			#print(cmd)
			os.system(cmd)
			
		
	#merge top/bottem binary	
	if type == "NonSecure":
		for sections in merge_tz:
			output_bin = output_dir+"/"+sections[0]+".bin"
			output_hex = output_dir+"/"+sections[0]+".hex "
			
			cmd = "rm -f "+output_bin+";"
			for i in range(1, len(sections)):						
				input_bin = output_dir+"/"+sections[i]+".bin"
				if os.path.isfile(input_bin):								
					cmd +=" cat "+input_bin+" >> "+output_bin+";"
					
			if sections[0].find("ddr") != -1:
				cmd += "hexdump -v -e '2/1 \"%02x \"' -e '\"\\n\"' "
				cmd += output_bin+" | awk '{ print $8$7$6$5$4$3$2$1 }'  > "+output_hex+";"				
			else:
				cmd += "hexdump -v -e '8/1 \"%02x \"' -e '\"\\n\"' "
				cmd += output_bin+" | awk '{ print $8$7$6$5$4$3$2$1 }'  > "+output_hex+";"
					
			#print(cmd)
			os.system(cmd)
			
if __name__ == "__main__":
    main()


