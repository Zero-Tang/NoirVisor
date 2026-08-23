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

def main():
	i=1
	optimizer_enabled=False
	target="uefi"
	# NoirVisor usually doesn't use more than 20KB of heap.
	# 256KiB of heap granularity should be quite enough.
	os.environ["DEFAULT_MMAP_GRANULARITY"]="0x40000"
	while i<len(sys.argv):
		if sys.argv[i]=="/target":
			i+=1
			config="build-{}.json".format(sys.argv[i])
			target=sys.argv[i]
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
	config="build-{}.json".format(target)
	if confirm_cargo():
		extra_vars_dict={"python":sys.executable}
		pl=Pipeline(config,optimizer_enabled,extra_vars=extra_vars_dict)
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