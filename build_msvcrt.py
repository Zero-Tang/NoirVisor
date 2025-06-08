#!/usr/bin/python3
import os
import platform
import shutil
import subprocess
import sys

from pipeline import Pipeline

def extract_lib(src:str,suffix:str,output_path:str):
	proc=subprocess.Popen(["lib","/LIST",src,"/NOLOGO"],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
	stdout_bytes,stderr_bytes=proc.communicate()
	stdout_text=stdout_bytes.decode()
	file_names=stdout_text.split('\r\n')
	L=[]
	for fn in file_names:
		if fn.endswith(suffix):
			L.append(fn)
	if len(L)==1:
		subprocess.call(["lib",src,"/EXTRACT:"+L[0],"/NOLOGO","/OUT:"+output_path])
	elif len(L)==0:
		print("Cannot find {} in {}!".format(suffix,src))
	else:
		print("Found more than one candidates: {}".format(L))

def main():
	outdir=None
	i=1
	while i<len(sys.argv):
		if sys.argv[i]=="--outdir":
			i+=1
			outdir=sys.argv[i]
		i+=1
	if not os.path.exists(os.path.join("bin",outdir,"msvcrt-static.lib")):
		sdk_path=os.environ["WindowsSdkDir"]
		if "WindowsSDKVersion" in os.environ:
			sdk_ver=os.environ["WindowsSDKVersion"]
		else:
			sdk_ver=os.environ["WindowsTargetPlatformVersion"]
		msvc_path=os.environ["VCToolsInstallDir"]
		inc_path=os.path.join(sdk_path,"Include",sdk_ver)
		lib_path=os.path.join(sdk_path,"Lib",sdk_ver)
		extract_lib(os.path.join(lib_path,"ucrt","x64","libucrt.lib"),"strlen.obj",os.path.join("bin",outdir,"Intermediate","strlen.obj"))
		extract_lib(os.path.join(msvc_path,"lib","x64","libcmt.lib"),"cpu_disp.obj",os.path.join("bin",outdir,"Intermediate","cpu_disp.obj"))
		shutil.copy2(os.path.join(msvc_path,"lib","x64","libcmt.amd64.pdb"),os.path.join("bin",outdir,"libcmt.amd64.pdb"))
		pl=Pipeline("build-msvcrt.json",outdir=outdir)
		pl.global_variable["incpath"]=inc_path
		pl.global_variable["crtpath"]=os.path.join(msvc_path,"crt","src","x64")
		pl.run()

if __name__=="__main__":
	if platform.system()!="Windows":
		print("{} is unsupported!".format(platform.system()))
		exit()
	main()