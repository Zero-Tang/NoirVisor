# This script will translate GNU "ar" command into MSVC "lib" command
import subprocess
import sys

if __name__=="__main__":
	i=1
	obj_files:list[str]=[]
	out_file:str|None=None
	while i<len(sys.argv):
		# Skip `cq` argument. MSVC lib does not recognize this argument.
		if sys.argv[i]=="cq":
			pass
		else:
			if out_file is None:
				out_file=sys.argv[i]
			else:
				obj_files.append(sys.argv[i])
		i+=1
	cmd_line=["lib"]+obj_files+["/NOLOGO","/OUT:"+out_file,"/MACHINE:X64","/ERRORREPORT:QUEUE"]
	subprocess.call(cmd_line)