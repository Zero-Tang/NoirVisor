import os
import sys
import platform
import subprocess
import time
from enum import Enum

# Pre-defined path
cl_offset="bin\\Hostx64\\x64"
# EWDK-22000
ddk_base_2019="T:\\"
ddk_path_2019="Program Files\\Microsoft Visual Studio\\2019\\BuildTools\\VC\\Tools\\MSVC\\14.28.29910"
sdk_path_2019="Program Files\\Windows Kits\\10\\bin\\10.0.22000.0\\x64"
inc_path_2019="Program Files\\Windows Kits\\10\\Include\\10.0.22000.0"
lib_path_2019="Program Files\\Windows Kits\\10\\Lib\\win7\\km\\x64"
# EWDK-22621
ddk_base_2022="V:\\"
ddk_path_2022="Program Files\\Microsoft Visual Studio\\2022\\BuildTools\\VC\\Tools\\MSVC\\14.31.31103"
sdk_path_2022="Program Files\\Windows Kits\\10\\bin\\10.0.22621.0\\x64"
inc_path_2022="Program Files\\Windows Kits\\10\\Include\\10.0.22621.0"
lib_path_2022="Program Files\\Windows Kits\\10\\Lib\\10.0.22621.0\\km\\x64"

class build_preset_kind(Enum):
	build_preset_core=0
	build_preset_uefi=1
	build_preset_win7=10
	build_preset_win11=11

	def mnemonic(preset):
		kind_dict={build_preset_kind.build_preset_uefi:"uefi",build_preset_kind.build_preset_win11:"win11",build_preset_kind.build_preset_win7:"win7"}
		return kind_dict[preset]

po_dict={"checked":False,"debug":False,"free":True,"release":True}
ps_dict={"win7":build_preset_kind.build_preset_win7,"win11":build_preset_kind.build_preset_win11,"uefi":build_preset_kind.build_preset_core,"core":build_preset_kind.build_preset_core}

# Resource unit, to be compiled by rc.exe
class resource_unit:
	def __init__(self,src_file:str,preset_kind:build_preset_kind,compiler:str="rc"):
		self.src_file:str=src_file
		self.preset:build_preset_kind=preset_kind
		self.compiler:str=compiler
		self.out_file:str=""
		self.parent:project_unit=None
		self.proc:subprocess.Popen=None
	
	def set_parent(self,parent)->None:
		self.parent:project_unit=parent

	def to_cmd(self)->list[str]:
		fn_base:str=os.path.split(self.src_file)[-1].split(".")[0]
		out_base:str=os.path.join(self.parent.out_dir,"Intermediate",fn_base)
		if self.preset==build_preset_kind.build_preset_win7:
			ddk_base=ddk_base_2019
			inc_path=inc_path_2019
		else:
			ddk_base=ddk_base_2022
			inc_path=inc_path_2022
		self.out_file=out_base+".res"
		cmd_line:list[str]=[self.compiler,"/nologo"]
		cmd_line.append("/i"+os.path.join(ddk_base,inc_path,"shared"))
		cmd_line.append("/i"+os.path.join(ddk_base,inc_path,"um"))
		cmd_line.append("/i"+os.path.join(ddk_base,inc_path,"km","crt"))
		cmd_line.append("/d_AMD64_")
		cmd_line.append("/fo{}".format(self.out_file))
		cmd_line+=["/n",self.src_file]
		return cmd_line

	def build(self)->None:
		cmd_line=self.to_cmd()
		print("[Resource] {}".format(cmd_line))
		self.proc=subprocess.Popen(cmd_line,stdout=subprocess.PIPE,stderr=subprocess.PIPE)

	def wait(self)->None:
		stdout_bytes:bytes
		stderr_bytes:bytes
		stdout_bytes,stderr_bytes=self.proc.communicate()
		if len(stdout_bytes):
			stdout_text=stdout_bytes.decode('latin-1')
			print("[Build - StdOut] Resource {}\n{}".format(self.src_file,stdout_text),end='')
		if len(stderr_bytes):
			stderr_text=stderr_bytes.decode('latin-1')
			print("[Build - StdErr] Resource {}\n{}".format(self.src_file,stderr_text),end='')

# Object unit, can be C or Assembly source
class object_unit:
	def __init__(self,src_file:str,preset_kind:build_preset_kind,optimize:bool=False,compiler:str="cl",extra_def:list[str]=[],force_std:bool=False):
		self.src_file=src_file
		self.preset=preset_kind
		self.use_emulator:bool=False
		self.is_disassembler:bool=False
		self.use_edk:bool=preset_kind==build_preset_kind.build_preset_uefi
		self.use_wdk:bool=preset_kind==build_preset_kind.build_preset_win7 or preset_kind==build_preset_kind.build_preset_win11
		self.use_nvbdk:bool=preset_kind==build_preset_kind.build_preset_core
		self.optimizer:bool=optimize
		self.parent:core_unit=None
		self.extra_def:list[str]=extra_def
		self.force_std:bool=force_std
		self.compiler:str=compiler
		self.out_file:str=""
	
	def set_parent(self,parent)->None:
		self.parent:core_unit=parent

	def to_cmd(self)->list[str]:
		fn_base:str=os.path.split(self.src_file)[-1].split(".")[0]
		out_base:str=os.path.join(self.parent.parent.out_dir,"Intermediate",self.parent.name,fn_base)
		if self.preset==build_preset_kind.build_preset_win7:
			ddk_base=ddk_base_2019
			ddk_path=ddk_path_2019
			inc_path=inc_path_2019
		else:
			ddk_base=ddk_base_2022
			ddk_path=ddk_path_2022
			inc_path=inc_path_2022
		self.out_file=out_base+".obj"
		if self.src_file.endswith(".c"):
			cmd_line:list[str]=[self.compiler,self.src_file]
			# Header files
			if self.use_emulator:
				cmd_line.append("/I"+os.path.join("..","src","disasm","zydis","include"))
				cmd_line.append("/I"+os.path.join("..","src","disasm","zydis","msvc"))
				cmd_line.append("/I"+os.path.join("..","src","disasm","zydis","dependencies","zycore","include"))
				if self.is_disassembler:
					cmd_line.append("/I"+os.path.join("..","src","disasm","zydis","src"))
			if self.use_wdk:
				cmd_line.append("/I"+os.path.join(ddk_base,inc_path,"km"))
				cmd_line.append("/I"+os.path.join(ddk_base,inc_path,"km","crt"))
				cmd_line.append("/I"+os.path.join(ddk_base,inc_path,"shared"))
			elif self.force_std:
				cmd_line.append("/I"+os.path.join(ddk_base,ddk_path,"include"))
			if self.use_nvbdk:
				cmd_line.append("/I"+os.path.join("..","src","include"))
			cmd_line+=["/Zi","/nologo","/W3","/WX"]
			if self.use_wdk:
				cmd_line.append("/wd4311")
			cmd_line+=["/O2"] if self.optimizer else ["/Oi","/O2" if self.optimizer else "/Od"]
			if self.use_emulator:
				cmd_line+=["/DZYDIS_STATIC_BUILD","/DZYAN_NO_LIBC"]
			if self.use_wdk:
				cmd_line+=["/D_AMD64_","/D_M_AMD64","/D_WIN64","/D_NDEBUG","/D_UNICODE","/DUNICODE","/D_KERNEL_MODE"]
			if self.use_nvbdk:
				cmd_line+=["/D_msvc","/D_amd64"]
			cmd_line.append("/D{}".format("_"+fn_base))
			cmd_line+=self.parent.extra_defs
			cmd_line+=self.extra_def
			cmd_line+=["/Zc:wchar_t","/std:c17","/FS","/FAcs"]
			cmd_line.append("/Fa{}.cod".format(out_base))
			cmd_line.append("/Fo{}.obj".format(out_base))
			cmd_line.append("/Fd{}{}vc143.pdb".format(os.path.join(self.parent.parent.out_dir,"Intermediate"),os.path.sep))
			cmd_line+=["/GS-","/Qspectre","/TC","/c","/errorReport:queue"]
		elif self.src_file.endswith(".asm"):
			cmd_line:list[str]=[self.compiler,"/W3","/WX","/D_amd64","/Zf","/Zd"]
			cmd_line.append("/Fo{}.obj".format(out_base))
			cmd_line+=["/c","/nologo",self.src_file]
		return cmd_line

	def build(self)->subprocess.Popen:
		return subprocess.Popen(self.to_cmd(),stdout=subprocess.PIPE,stderr=subprocess.PIPE)

# Core unit, can be a set of objects.
class core_unit:
	def __init__(self,name:str,parent,preset_kind:build_preset_kind,optimize:bool=False):
		ps_dict:dict[build_preset_kind,str]={build_preset_kind.build_preset_win7:"win7",build_preset_kind.build_preset_win11:"win11",build_preset_kind.build_preset_uefi:"uefi"}
		self.name:str=name
		self.parent:project_unit=parent
		self.objects:dict[str:object_unit]={}
		self.extra_defs:list[str]=[]
		self.preset:build_preset_kind=preset_kind
		self.out_dir:str=os.path.join("..","bin","comp{}_{}x64".format("fre" if optimize else "chk",ps_dict[preset_kind]),"Intermediate",self.name)
		self.out_file:str=os.path.join(self.out_dir,self.name)+".lib"
		self.proc_dict:dict[str,subprocess.Popen]={}
		self.parent.add_core(self)
	
	def build(self)->None:
		print("[mkdir] {}".format(self.out_dir))
		try:
			os.makedirs(self.out_dir,exist_ok=True)
		except:
			pass
		for obj_name in self.objects:
			obj=self.objects[obj_name]
			print("[Core {}] Building {}...".format(self.name,obj.src_file))
			print("[Core {} | {}] Command: {}".format(self.name,obj.src_file,obj.to_cmd()))
			self.proc_dict[obj_name]=obj.build()
	
	def wait(self)->None:
		for obj_name in self.proc_dict:
			proc=self.proc_dict[obj_name]
			stdout_bytes:bytes
			stderr_bytes:bytes
			stdout_bytes,stderr_bytes=proc.communicate()
			if len(stdout_bytes):
				stdout_text=stdout_bytes.decode('latin-1')
				print("[Build - StdOut] Object {}\n{}".format(obj_name,stdout_text),end='')
			if len(stderr_bytes):
				stderr_text=stderr_bytes.decode('latin-1')
				print("[Build - StdErr] Object {}\n{}".format(obj_name,stderr_text),end='')

	def add_object(self,object:object_unit,name:str=None)->None:
		if name is None:
			name=os.path.split(object.src_file)[-1].split('.')[0]
		if name in self.objects:
			print("Error! Object {} was already added!".format(name))
			exit(1)
		self.objects[name]=object
		object.set_parent(self)

# Project unit, this will output the final executable.
class project_unit:
	def __init__(self,name:str,preset:build_preset_kind,optimizer:bool=False,output_lib:bool=False):
		self.name:str=name
		self.preset:build_preset_kind=preset
		self.optimizer:bool=optimizer
		self.output_lib:bool=output_lib
		self.out_dir:str=os.path.join("..","bin","comp{}_{}x64".format("fre" if optimizer else "chk",build_preset_kind.mnemonic(preset)))
		self.cores:set[core_unit]=set()
		self.resources:dict[str,resource_unit]={}
		self.extra_lib:list[str]=[]
	
	def add_core(self,core:core_unit)->None:
		self.cores.add(core)

	def add_resource(self,name:str,resource:resource_unit)->None:
		self.resources[name]=resource

	def build(self)->subprocess.Popen:
		if self.output_lib:
			cmd_line:list[str]=["lib"]
			# Call compilers asynchronously.
			for core in self.cores:
				core.build()
			for res_name in self.resources:
				self.resources[res_name].build()
			# Wait for compilers to finish
			for core in self.cores:
				core.wait()
				for obj_name in core.objects:
					obj:object_unit=core.objects[obj_name]
					cmd_line.append(obj.out_file)
			cmd_line+=["/NOLOGO","/OUT:{}.lib".format(os.path.join(self.out_dir,self.name)),"/MACHINE:X64","/ERRORREPORT:QUEUE"]
		else:
			# Initialize all path variables.
			if self.preset==build_preset_kind.build_preset_win7:
				ddk_base=ddk_base_2019
				lib_path=lib_path_2019
			else:
				ddk_base=ddk_base_2022
				lib_path=lib_path_2022
			cmd_line:list[str]=["link"]
			# Call compilers asynchronously
			for core in self.cores:
				core.build()
			for res_name in self.resources:
				self.resources[res_name].build()
			# Wait for compilers to finish
			for core in self.cores:
				core.wait()
				for obj_name in core.objects:
					obj:object_unit=core.objects[obj_name]
					cmd_line.append(obj.out_file)
			for res_name in self.resources:
				res=self.resources[res_name]
				res.wait()
				cmd_line.append(res.out_file)
			# Finish up the linker arguments
			cmd_line+=["/NOLOGO","/NODEFAULTLIB"]
			if self.preset==build_preset_kind.build_preset_win7 or self.preset==build_preset_kind.build_preset_win11:
				cmd_line.append("/LIBPATH:{}".format(os.path.join(ddk_base,lib_path)))
				cmd_line+=["ntoskrnl.lib","hal.lib"]
			cmd_line+=self.extra_lib
			cmd_line+=["/DEBUG","/PDB:{}".format(os.path.join(self.out_dir,"NoirVisor.pdb"))]
			if self.preset==build_preset_kind.build_preset_win7 or self.preset==build_preset_kind.build_preset_win11:
				cmd_line.append("/OUT:{}".format(os.path.join(self.out_dir,"{}.sys".format(self.name))))
				cmd_line+=["/SUBSYSTEM:NATIVE","/Driver","/ENTRY:NoirDriverEntry"]
			if self.optimizer:
				cmd_line+=["/OPT:REF","/OPT:ICF"]
			cmd_line+=["/Machine:X64","/ERRORREPORT:QUEUE"]
			print("[Linker] {}".format(cmd_line))
		# Call Linker
		proc=subprocess.Popen(cmd_line,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
		return proc

# Sign an executable.
def sign_file(executable_fn:str,signature_fn:str)->int:
	cmd_line=["signtool","sign","/v","/fd","SHA1","/f",signature_fn,executable_fn]
	proc=subprocess.Popen(cmd_line,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
	stdout_bytes:bytes
	stderr_bytes:bytes
	stdout_bytes,stderr_bytes=proc.communicate()
	if len(stdout_bytes):
		stdout_text=stdout_bytes.decode('latin-1')
		print("[Linker - StdOut] ",stdout_text,end='')
	if len(stderr_bytes):
		stderr_text=stderr_bytes.decode('latin-1')
		print("[Linker - StdErr] ",stderr_text,end='')
	return proc.returncode

# Build the hypervisor
def build_hvm(preset_sys:str="win7",preset_opt:str="checked")->None:
	if not preset_sys in ps_dict:
		print("Unknown Target Platform: {}!".format(preset_sys))
	# Create project unit
	hv_proj=project_unit("NoirVisor",ps_dict[preset_sys],po_dict[preset_opt])
	# Create resourece unit
	ver_res:resource_unit=None
	if preset_sys.startswith("win"):
		ver_res=resource_unit(os.path.join("..","src","booting","windrv","version.rc"),ps_dict[preset_sys])
		ver_res.set_parent(hv_proj)
	# Create core objects
	boot_core=core_unit("booting",hv_proj,ps_dict[preset_sys],po_dict[preset_opt])
	emu_core=core_unit("emulator",hv_proj,ps_dict[preset_sys],po_dict[preset_opt])
	xpf_core=core_unit("platform",hv_proj,ps_dict[preset_sys],po_dict[preset_opt])
	mshv_core=core_unit("ms-hyper-v",hv_proj,ps_dict[preset_sys],po_dict[preset_opt])
	drv_core=core_unit("drivers",hv_proj,ps_dict[preset_sys],po_dict[preset_opt])
	vt_core=core_unit("intel-vt",hv_proj,ps_dict[preset_sys],po_dict[preset_opt])
	svm_core=core_unit("amd-v",hv_proj,ps_dict[preset_sys],po_dict[preset_opt])
	# Initialize Booting Core object
	bootdir_dict={"win7":"windrv","win11":"windrv","uefi":"efiapp"}
	boot_core.add_object(object_unit(os.path.join("..","src","booting",bootdir_dict[preset_sys],"driver.c"),ps_dict[preset_sys],po_dict[preset_opt]))
	# Initialize Emulator Core object
	for fn in os.listdir(os.path.join("..","src","disasm")):
		if fn.endswith(".c"):
			obj=object_unit(os.path.join("..","src","disasm",fn),build_preset_kind.build_preset_core,po_dict[preset_opt])
			obj.use_emulator=True
			emu_core.add_object(obj)
	# Initialize Cross-Platform Core object
	xpfdir_dict={"win7":"windows","win11":"windows","uefi":"uefi"}
	for fn in os.listdir(os.path.join("..","src","xpf_core")):
		if fn.endswith(".c"):
			xpf_core.add_object(object_unit(os.path.join("..","src","xpf_core",fn),build_preset_kind.build_preset_core,po_dict[preset_opt]))
	for fn in os.listdir(os.path.join("..","src","xpf_core",xpfdir_dict[preset_sys])):
		if fn.endswith(".c") or fn.endswith(".asm"):
			xpf_core.add_object(object_unit(os.path.join("..","src","xpf_core",xpfdir_dict[preset_sys],fn),ps_dict[preset_sys],po_dict[preset_opt],"cl" if fn.endswith(".c") else "ml64"))
	for fn in os.listdir(os.path.join("..","src","xpf_core","msvc")):
		if fn.endswith(".asm"):
			xpf_core.add_object(object_unit(os.path.join("..","src","xpf_core","msvc",fn),build_preset_kind.build_preset_core,po_dict[preset_opt],"ml64"))
	xpf_core.objects["ci"].extra_def=["/D_code_integrity"]
	xpf_core.objects["noirhvm"].extra_def=["/D_central_hvm"]
	xpf_core.objects["devkits"].extra_def=["/D_dev_kits"]
	xpf_core.objects["devkits"].force_std=True
	xpf_core.objects["nvdbg"].force_std=True
	# Initialize Microsoft Hypervisor Core object
	mshv_core.extra_defs=["/D_mshv_core"]
	for fn in os.listdir(os.path.join("..","src","mshv_core")):
		if fn.endswith(".c"):
			mshv_core.add_object(object_unit(os.path.join("..","src","mshv_core",fn),build_preset_kind.build_preset_core,po_dict[preset_opt]))
	# Initialize Driver Core object
	for dn in os.listdir(os.path.join("..","src","drv_core")):
		if os.path.isdir(os.path.join("..","src","drv_core",dn)):
			for fn in os.listdir(os.path.join("..","src","drv_core",dn)):
				if fn.endswith(".c"):
					drv_core.add_object(object_unit(os.path.join("..","src","drv_core",dn,fn),build_preset_kind.build_preset_core,po_dict[preset_opt]))
	# Initialize Intel VT-x Core object
	vt_core.extra_defs=["/D_vt_core"]
	for fn in os.listdir(os.path.join("..","src","vt_core")):
		if fn.endswith(".c"):
			vt_core.add_object(object_unit(os.path.join("..","src","vt_core",fn),build_preset_kind.build_preset_core,po_dict[preset_opt]))
	# Initialize AMD-V Core object
	svm_core.extra_defs=["/D_svm_core"]
	for fn in os.listdir(os.path.join("..","src","svm_core")):
		if fn.endswith(".c"):
			svm_core.add_object(object_unit(os.path.join("..","src","svm_core",fn),build_preset_kind.build_preset_core,po_dict[preset_opt]))
	# Add resource to project
	if preset_sys.startswith("win"):
		print("Adding resource {}...".format(ver_res.src_file))
		hv_proj.add_resource("version",ver_res)
	# Start Building
	hv_proj.extra_lib=[os.path.join(hv_proj.out_dir,"zydis.lib")]
	proc=hv_proj.build()
	stdout_bytes:bytes
	stderr_bytes:bytes
	stdout_bytes,stderr_bytes=proc.communicate()
	if len(stdout_bytes):
		stdout_text=stdout_bytes.decode('latin-1')
		print("[Linker - StdOut] ",stdout_text,end='')
	if len(stderr_bytes):
		stderr_text=stderr_bytes.decode('latin-1')
		print("[Linker - StdErr] ",stderr_text,end='')
	# As NoirVisor is built, sign the executable.
	if preset_sys.startswith("win"):
		if not po_dict[preset_opt]:
			sign_file(os.path.join(hv_proj.out_dir,"NoirVisor.sys"),"ztnxtest.pfx")

# Build the disassembler
def build_disasm(preset_sys:str="win7",preset_opt:str="checked")->None:
	# Create project unit
	disasm_proj=project_unit("zydis",ps_dict[preset_sys],po_dict[preset_opt],True)
	# Create core objects
	disasm_core=core_unit("zydis",disasm_proj,ps_dict[preset_sys],po_dict[preset_opt])
	# Initialize core object
	disasm_path_base=os.path.join("..","src","disasm","zydis","src")
	for fn in os.listdir(disasm_path_base):
		if fn.endswith(".c"):
			obj=object_unit(os.path.join(disasm_path_base,fn),build_preset_kind.build_preset_core,po_dict[preset_opt])
			obj.use_emulator=True
			obj.is_disassembler=True
			disasm_core.add_object(obj)
	# Start Building
	proc=disasm_proj.build()
	stdout_bytes:bytes
	stderr_bytes:bytes
	stdout_bytes,stderr_bytes=proc.communicate()
	if len(stdout_bytes):
		stdout_text=stdout_bytes.decode('latin-1')
		print("[Linker - StdOut] ",stdout_text,end='')
	if len(stderr_bytes):
		stderr_text=stderr_bytes.decode('latin-1')
		print("[Linker - StdErr] ",stderr_text,end='')

def cmd_build(presets:list[str])->bool:
	t1=time.time()
	preset_sys="win7"
	preset_opt="checked"
	preset_tar="hvm"
	# Parse arguments
	i=0
	while i<len(presets):
		if presets[i]=="/platform":
			i+=1
			preset_sys=presets[i]
		elif presets[i]=="/optimize":
			i+=1
			preset_opt=presets[i]
		elif presets[i]=="/target":
			i+=1
			preset_tar=presets[i]
		else:
			print("Error: Unknown argument: {}!".format(presets[i]))
			return False
		i+=1
	# Initialize Environment Variable
	if preset_sys=="win7":
		ddk_base=ddk_base_2019
		ddk_path=ddk_path_2019
		sdk_path=sdk_path_2019
	else:
		ddk_base=ddk_base_2022
		ddk_path=ddk_path_2022
		sdk_path=sdk_path_2022
	os.environ["PATH"]+=";{}{}{}{}".format(ddk_base,ddk_path,os.path.sep,cl_offset)
	os.environ["PATH"]+=";{}{}{}".format(ddk_base,os.path.sep,sdk_path)
	# Dispatch by the target
	if preset_tar=="hvm":
		build_hvm(preset_sys,preset_opt)
	elif preset_tar=="disasm":
		build_disasm(preset_sys,preset_opt)
	else:
		print("Unknown target: {}!".format(preset_tar))
	t2=time.time()
	print("Build time elapsed: {} seconds".format(t2-t1))
	# Build Ends
	return True

def cmd_clean(cmd_list:list[str])->bool:
	print("Cleaning is not implemented!")

def cmd_shell(cmd_list:list[str])->bool:
	print("NoirVisor Build System Shell is not implemented!")

if __name__=="__main__":
	if platform.system()=="Windows":
		cmd_dict:dict={"build":cmd_build,"shell":cmd_shell,"clean":cmd_clean}
		if len(sys.argv)==1:
			cmd_build(sys.argv[2:])
		else:
			cmd_str=sys.argv[1].lower()
			if cmd_str in cmd_dict:
				cmd_dict[cmd_str](sys.argv[2:])
			else:
				print("Unknown command: {}".format(sys.argv[1]))
	else:
		print("Unsupported Platform: {} {}".format(platform.system(),platform.version()))