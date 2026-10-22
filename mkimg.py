#!/usr/bin/python3
import subprocess
import sys

def cmd_exists(cmd:list[str],expectation:str)->bool:
	try:
		proc=subprocess.Popen(cmd,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
		stdout_bytes,stderr_bytes=proc.communicate()
		if stdout_bytes.decode().startswith(expectation):
			return True
	except:
		pass
	return False

def confirm_mtools()->bool:
	a=cmd_exists(["mformat","--version"],"mformat (GNU mtools)")
	b=cmd_exists(["mmd","--version"],"mmd (GNU mtools)")
	c=cmd_exists(["mcopy","--version"],"mcopy (GNU mtools)")
	return a and b and c

if __name__=="__main__":
	if confirm_mtools():
		# Parse arguments
		i=1
		image_size:int=2880<<10
		sector_num:int=image_size>>9
		output_file:str=None
		mkdir_orders:list[str]=[]
		copy_orders:dict[str,str]=dict()
		while i<len(sys.argv):
			if sys.argv[i]=="-o" or sys.argv[i]=="--output":
				i+=1
				if output_file is None:
					output_file=sys.argv[i]
				else:
					print("Output disk image file is specified more than once!")
			elif sys.argv[i]=="-md" or sys.argv[i]=="--mkdir":
				i+=1
				mkdir_orders.append(sys.argv[i])
			elif sys.argv[i]=="-c" or sys.argv[i]=="--copy":
				i+=1
				tmp=sys.argv[i].split('|')
				copy_orders[tmp[0]]=tmp[1]
			elif sys.argv[i]=="-s" or sys.argv[i]=="--size":
				i+=1
				size=sys.argv[i]
				img_size_str=size
				mult=1
				if size[-1].upper()=="G":
					mult=1024**3
					size=size[:-1]
				elif size[-1].upper()=="M":
					mult=1024**2
					size=size[:-1]
				elif size[-1].upper()=="K":
					mult=1024**1
					size=size[:-1]
				image_size=int(float(size)*mult)
				sector_num=image_size>>9
			else:
				print("Ignoring unknown argument {}!".format(sys.argv[i]))
			i+=1
		# Create Disk Image.
		with open(output_file,'wb') as f:
			f.write(b'\0'*image_size)
			f.close()
		# Format Disk Image.
		subprocess.call(["mformat","-i",output_file,"-v","NoirVisor","-T",str(sector_num),"::"])
		# Make Directories.
		for md in mkdir_orders:
			subprocess.call(["mmd","-i",output_file,md])
		# Copy Files.
		for fn in copy_orders:
			subprocess.call(["mcopy","-i",output_file,fn,copy_orders[fn]])
	else:
		print("GNU mtools are missing! Add them to the PATH in order to build a raw NoirVisor disk image!")