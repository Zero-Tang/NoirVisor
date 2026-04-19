#!/usr/bin/python3
import os
import platform
import subprocess
import sys
import time

from pipeline import Pipeline

def confirm_cargo()->bool:
	proc=subprocess.Popen(["cargo","--version"],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
	try:
		stdout_bytes,stderr_bytes=proc.communicate()
		stdout_text=stdout_bytes.decode()
		if not stdout_text.startswith("cargo"):
			print("This cargo doesn't seem right...")
			return False
		cargo_ver=stdout_text[:-1]
	except:
		print("Cargo does not exist! Did you install rust-lang?")
		return False
	proc=subprocess.Popen(["rustc","--version"],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
	try:
		stdout_bytes,stderr_bytes=proc.communicate()
		stdout_text=stdout_bytes.decode()
		if not stdout_text.startswith("rustc"):
			print("This rustc doesn't seem right...")
			return False
		rustc_ver=stdout_text[:-1]
	except:
		print("rustc does not exist! Did you install rust-lang?")
		return False
	print("We are using {} and {}...".format(cargo_ver,rustc_ver))
	return True

def confirm_msvc()->bool:
	try:
		sdk_path=os.environ["WindowsSdkDir"]
		if "WindowsSDKVersion" in os.environ:
			sdk_ver=os.environ["WindowsSDKVersion"]
		else:
			sdk_ver=os.environ["WindowsTargetPlatformVersion"]
	except:
		print("Windows SDK is not installed! Make sure you are calling this script from MSVC 2022 Command Prompt!")
		return False
	try:
		msvc_path=os.environ["VCToolsInstallDir"]
	except:
		print("MSVC is not installed! Make sure you are calling this script from MSVC 2022 Command Prompt!")
		return False
	print("MSVC is installed at {}".format(msvc_path))
	print("Windows SDK Version: {}".format(sdk_ver[:-1]))
	print("Windows Kits is installed at {}".format(sdk_path))
	inc_path=os.path.join(sdk_path,"Include",sdk_ver)
	if os.path.exists(inc_path):
		if not os.path.exists(os.path.join(inc_path,"km")):
			print("Error: Missing WDK Header Files! Did you install Windows Driver Kits?")
	else:
		print("Missing C/C++ Include Path!")
		return False
	lib_path=os.path.join(sdk_path,"Lib",sdk_ver)
	if os.path.exists(lib_path):
		if not os.path.exists(os.path.join(lib_path,"km")):
			print("Error: Missing WDK Import Libraries! Did you install Windows Driver Kits?")
			return False
	else:
		print("Missing Library Path!")
		return False
	return True

def main():
	i=1
	config="build-windows.json"
	optimizer_enabled=False
	target="unknown"
	# NoirVisor usually doesn't use more than 20KB of heap.
	# 256KiB of heap granularity should be quite enough.
	os.environ["DEFAULT_MMAP_GRANULARITY"]="0x40000"
	while i<len(sys.argv):
		if sys.argv[i]=="/target":
			i+=1
			config="build-{}.json".format(sys.argv[i])
			target=sys.argv[1]
		elif sys.argv[i].startswith("/opt:"):
			expr=sys.argv[i][5:]
			if expr.lower()=="yes" or expr.lower()=="true":
				optimizer_enabled=True
			elif expr.lower()=="no" or expr.lower()=="false":
				optimizer_enabled=False
			else:
				print("Ignoring unknown optimization argument {}!".format(sys.argv[i]))
		else:
			print("Ignoring unknown argument {}!".format(sys.argv[i]))
		i+=1
	if confirm_cargo() and confirm_msvc():
		sdk_path=os.environ["WindowsSdkDir"]
		if "WindowsSDKVersion" in os.environ:
			sdk_ver=os.environ["WindowsSDKVersion"]
		else:
			sdk_ver=os.environ["WindowsTargetPlatformVersion"]
		msvc_path=os.environ["VCToolsInstallDir"]
		inc_path=os.path.join(sdk_path,"Include",sdk_ver)
		lib_path=os.path.join(sdk_path,"Lib",sdk_ver)
		pl=Pipeline(config,optimizer_enabled,extra_vars={"ddkpath":msvc_path,"incpath":inc_path,"libpath":lib_path})
		pl.run()

if __name__=="__main__":
	known_platforms={"Windows","Linux"}
	if not platform.system() in known_platforms:
		print("Host OS {} is unsupported by this build script!".format(platform.system()))
		exit()
	t1:float=time.time()
	main()
	t2:float=time.time()
	print("{} seconds spent in compilation!".format(t2-t1))