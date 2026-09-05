import fnmatch
import os
import platform
import subprocess
import sys

def call_bochs():
	bochs_path=os.path.join(progpath,L[0])
	print("Detected Bochs installed at {}!".format(bochs_path))
	old_path=os.environ["PATH"]
	os.environ["PATH"]+=";"+bochs_path
	var_dict={"cpu-model":"wildcat_lake","bochs-path":bochs_path,"build-preset":"chk"}
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
		else:
			print("Unknown argument: {}!".format(sys.argv[i]))
		i+=1
	print(var_dict)
	# Fill the Bochs configuration template.
	f=open("bochs-template.txt",'r')
	bochs_config=f.read()
	f.close()
	bochs_config=bochs_config.format(**var_dict)
	f=open("bochsrc.bxrc",'w')
	f.write(bochs_config)
	f.close()
	# Run Bochs.
	subprocess.call(["bochs","-f","bochsrc.bxrc","-q"])
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
	else:
		print("This system ({}) is not supported yet!".format(platform.system()))