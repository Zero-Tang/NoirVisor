import fnmatch
import os
import platform
import subprocess
import sys

def call_bochs():
	# OVMF must be present.
	subprocess.call([sys.executable,"download-ovmf.py","bochs"])
	bochs_path=os.path.join(progpath,L[0])
	print("Detected Bochs installed at {}!".format(bochs_path))
	old_path=os.environ["PATH"]
	os.environ["PATH"]+=";"+bochs_path
	var_dict={"cpu-model":"wildcat_lake","bochs-path":bochs_path,"build-preset":"chk"}
	if platform.system()=="Windows":
		var_dict["displib"]="win32"
	else:
		var_dict["displib"]="sdl2"
	debugger_en=False
	auto_check=False
	# Parse Command-Line Arguments
	i=1
	while i<len(sys.argv):
		if sys.argv[i]=="--cpu-model":
			i+=1
			# Recommended options are:
			# To emulate Intel: "wildcat_lake" (default)
			# To emulate AMD: "ryzen"
			# For full list of CPU models supported by Bochs, check Bochs' documentation:
			# https://bochs.sourceforge.io/doc/docbook/user/cpu-models.html
			var_dict["cpu-model"]=sys.argv[i]
		elif sys.argv[i]=="--release":
			var_dict["build-preset"]="fre"
		elif sys.argv[i]=="--debugger":
			debugger_en=True
		elif sys.argv[i]=="--nographic":
			var_dict["displib"]="nogui"
		elif sys.argv[i]=="--auto-check":
			auto_check=True
		else:
			print("Unknown argument: {}!".format(sys.argv[i]))
		i+=1
	if auto_check:
		var_dict["displib"]="nogui"
	print(var_dict)
	# Fill the Bochs configuration template.
	f=open("bochs-template.txt",'r')
	bochs_config=f.read()
	f.close()
	bochs_config=bochs_config.format(**var_dict)
	f=open("bochsrc.bxrc",'w')
	f.write(bochs_config)
	f.close()
	# Forge Bochs run arguments
	args=["bochs","-f","bochsrc.bxrc","-q"]
	if debugger_en:
		args.append("-dbg_gui")
	# Run Bochs.
	if auto_check:
		proc=subprocess.Popen(args,stdout=subprocess.PIPE)
		output,_=proc.communicate()
		output=output.decode("utf-8","replace")
		sys.stdout.flush()
		sys.stdout.write(output)
		# Check the outputs.
		# Check if the system subversion completes. Also checks the CI-fault test.
		i=output.find("System subversion completed!")
		assert i!=-1
		assert output.find("CI-fault",i)!=-1
		print("Bochs Check passed!")
	else:
		subprocess.call(args)
	os.environ["PATH"]=old_path

if __name__=="__main__":
	# OVMF must be present.
	subprocess.call([sys.executable,"download-ovmf.py"])
	# Search for Bochs Path.
	if platform.system()=="Windows":
		progpath=os.environ["ProgramFiles"]
		L=fnmatch.filter(os.listdir(progpath),"Bochs-*")
		if len(L)==0:
			print("No Bochs installations are found!")
		elif len(L)>1:
			print("More than one Bochs installations are found:")
			for s in L:
				print(os.path.join(progpath,s))
			L.sort(reverse=True)
			print("Invoking latest Bochs version...")
			call_bochs()
		else:
			call_bochs()
	elif platform.system()=="Linux":
		if not os.path.isdir("/usr/share/bochs"):
			print("No Bochs installations are found!")
			exit(1)
		progpath="/usr/share"
		L=["bochs"]
		call_bochs()
	else:
		print("This system ({}) is not supported yet!".format(platform.system()))