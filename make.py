#!/usr/bin/python3
import json
import os
import platform
import subprocess
import sys
import time

verbose:bool=False

class compiler_unit:
	def __init__(self,definition:dict,platform:str):
		global verbose
		self.def_dict=definition
		self.family:str=definition["family"]
		tmp_dict:dict={}
		if self.family=="msvc":
			# Paths
			self.ddk_path=definition["ddk_path"]
			self.sdk_path=definition["sdk_path"]
			tmp_dict={"ddk_path":self.ddk_path,"sdk_path":self.sdk_path}
			try:
				self.tool_paths_amd64:list[str]=definition["tool_paths.{}".format(platform)]
			except KeyError:
				print("No Tool Paths for {} is found!".format(platform))
			# Add to environment variables.
			for path in self.tool_paths_amd64:
				new_path="{};".format(path.format(**tmp_dict))
				os.environ['PATH']=new_path+os.environ['PATH']
			self.cl_flag:list[str]=definition["cl_flags"]
			# Resource Compilers.
			self.rc_flag:list[str]=definition["rc_flags"]
			self.include_rcflag:str=definition["include_rcflag"]
			self.preproc_def_rcflag:str=definition["preproc_def_rcflag"]
			self.resource_output:str=definition["resource_output"]
			# Macro Assemblers.
			self.ml_flag:list[str]=definition["ml_flags"]
			self.include_mlflag:str=definition["include_mlflag"]
			self.preproc_def_mlflag:str=definition["preproc_def_mlflag"]
			# Symbol
			self.symbol_output:str=definition["symbol_output"]
			# WDK Includes.
			self.wdk_incpath:list[str]=[s.format(**tmp_dict) for s in definition["wdk_incpath"]]
			self.sdk_incpath:list[str]=[s.format(**tmp_dict) for s in definition["sdk_incpath"]]
			self.wdk_libpath:list[str]=[s.format(**tmp_dict) for s in definition["wdk_libpath.amd64"]]
		self.libc_incpath:str=definition["libc_incpath"]
		self.libc_incpath=self.libc_incpath.format(**tmp_dict)
		self.os:str=definition["os"]
		self.noopt_flags:list[str]=definition["noopt_flags"]
		self.opt_flags:list[str]=definition["opt_flags"]
		self.preproc_def_flag:str=definition["preproc_def_flag"]
		self.include_flag:str=definition["include_flag"]
		self.assembly_output:str=definition["assembly_output"]
		self.object_output:str=definition["object_output"]
		self.linker_flags:list[str]=definition["linker_flags"]
		try:
			self.linker_flags+=definition["linker_flags.{}".format(platform)]
		except:
			print("No {}-specific linker flags!".format(platform))
		self.lib_flags:list[str]=definition["lib_flags"]
		try:
			self.lib_flags+=definition["lib_flags.{}".format(platform)]
		except:
			print("No {}-specific lib flags!".format(platform))
		self.libpath_flag:str=definition["libpath_flag"]
		self.binary_output:str=definition["binary_output"]
		self.entry_point:str=definition["entry_point"]

class object_unit:
	# Each object unit represents a source file. (*.c or *.asm)
	def __init__(self,file_name:str,compiler:compiler_unit,file_type:str,output_dir:str,includes:list[str],flags:list[str]):
		self.file_name:str=file_name
		self.compiler=compiler
		self.cmd:list[str]=[]
		self.process:subprocess.Popen=None
		self.stdout_text:str=""
		self.stderr_text:str=""
		self.file_type=file_type
		self.target_name:str=".".join(os.path.split(file_name)[-1].split('.')[0:-1])
		dict_pool:dict={"output_base":output_dir}
		dict_pool["target_name"]=self.target_name
		# FIXME: Architecture keywords
		dict_pool["arch"]="amd64"
		dict_pool["ARCH"]="AMD64"
		dict_pool["compiler_family"]=compiler.family
		dict_pool["extension"]=os.path.split(file_name)[-1].split('.')[-1]
		# Initialize the command-line arguments
		# Compiler executable
		if compiler.family=="msvc":
			if file_type=="C":
				self.cmd.append("cl")
				self.cmd.append(file_name)
				for inc in includes:
					self.cmd.append(compiler.include_flag.format(inc))
				for flag in flags:
					self.cmd.append(compiler.preproc_def_flag.format(flag.format(**dict_pool)))
				for flag in compiler.cl_flag:
					self.cmd.append(flag.format(**dict_pool))
				self.cmd.append(compiler.assembly_output.format(**dict_pool))
				self.cmd.append(compiler.object_output.format(**dict_pool))
			elif file_type=="Asm":
				self.cmd.append("ml64")
				for flag in flags:
					self.cmd.append(compiler.preproc_def_mlflag.format(flag.format(**dict_pool)))
				for flag in compiler.ml_flag:
					self.cmd.append(flag.format(**dict_pool))
				self.cmd.append(compiler.object_output.format(**dict_pool))
				self.cmd.append(file_name)
			elif file_type=="Res":
				self.cmd.append("rc")
				for inc in includes:
					self.cmd.append(compiler.include_rcflag.format(inc))
				for flag in flags:
					self.cmd.append(compiler.preproc_def_rcflag.format(flag.format(**dict_pool)))
				for flag in compiler.rc_flag:
					self.cmd.append(flag.format(**dict_pool))
				self.cmd.append(compiler.resource_output.format(**dict_pool))
				self.cmd.append(file_name)
			else:
				print("Unknown file type: {}".format(file_type))
		else:
			print("Unknown compiler family: {}!".format(compiler.family))
	
	def build(self)->None:
		global verbose
		if verbose:
			print("[Build - {}] {}".format(self.file_name,self.cmd))
		self.process=subprocess.Popen(self.cmd,stdout=subprocess.PIPE,stderr=subprocess.PIPE)

	def wait(self,timeout:float=None)->int:
		stdout_bytes:bytes
		stderr_bytes:bytes
		stdout_bytes,stderr_bytes=self.process.communicate(timeout=timeout)
		self.stdout_text=stdout_bytes.decode()
		self.stderr_text=stderr_bytes.decode()
		return self.process.wait(timeout)

class manifest_unit:
	# Each manifest unit represents a build.json file.
	def __init__(self,file_name:str,target:str,compiler:compiler_unit,platform:str):
		global verbose
		self.file_name:str=file_name
		self.dir_base:str=os.sep.join(os.path.split(self.file_name)[0:-1])
		self.subordinate_manifests:list[manifest_unit]=[]
		self.c_sources:list[str]=[]
		self.asm_sources:list[str]=[]
		self.rc_sources:list[str]=[]
		f=open(self.file_name,'r')
		dict_str=f.read()
		f.close()
		manifest_dict:dict=json.loads(dict_str)
		self.c_includes:list[str]=[]
		self.cflags:list[str]=[]
		self.cflags_per_file:dict={}
		self.aflags:list[str]=[]
		self.platform_per_file:dict={}
		# Internal dictionary is required for format specifiers in config files.
		self._internal_dict:dict={"libc":compiler.libc_incpath}
		if target in manifest_dict:
			# Manifest has a tree structure.
			if "manifests" in manifest_dict[target]:
				sub_manifests:list[str]=manifest_dict[target]["manifests"]
				for subm in sub_manifests:
					new_subm_fn=os.path.join(self.dir_base,os.sep.join(subm.split('/')))
					if verbose:
						print("Subordinate Manifest: {}".format(new_subm_fn))
					if os.path.exists(new_subm_fn):
						self.subordinate_manifests.append(manifest_unit(new_subm_fn,target,compiler,platform))
					else:
						print("Warning: Manifest file {} does not exist!".format(new_subm_fn))
			platform_specific_manifest:str="manifests."+platform
			if platform_specific_manifest in manifest_dict[target]:
				sub_manifests:list[str]=manifest_dict[target][platform_specific_manifest]
				for subm in sub_manifests:
					new_subm_fn=os.path.join(self.dir_base,os.sep.join(subm.split('/')))
					if verbose:
						print("Subordinate Manifest of Platform {}: {}".format(platform,new_subm_fn))
					if os.path.exists(new_subm_fn):
						self.subordinate_manifests.append(manifest_unit(new_subm_fn,target,compiler,platform))
					else:
						print("Warning: Manifest file {} does not exist!".format(new_subm_fn))
			# List source files
			if "c_sources" in manifest_dict[target]:
				src:str
				for src in manifest_dict[target]["c_sources"]:
					self.c_sources.append(os.path.join(self.dir_base,os.sep.join(src.split('/'))))
			if "asm_sources" in manifest_dict[target]:
				src:str
				for src in manifest_dict[target]["asm_sources"]:
					self.asm_sources.append(os.path.join(self.dir_base,os.sep.join(src.split('/'))))
			if "res_sources" in manifest_dict[target]:
				src:str
				for src in manifest_dict[target]["res_sources"]:
					self.rc_sources.append(os.path.join(self.dir_base,os.sep.join(src.split('/'))))
			# List include headers
			if "c_includes" in manifest_dict[target]:
				inc_list:list[str]=manifest_dict[target]["c_includes"]
				for inc in inc_list:
					self.c_includes.append(inc.format(**self._internal_dict).replace('/',os.sep))
			# Pre-processor Definitions
			if "extra_preproc_defflag" in manifest_dict[target]:
				self.cflags:list[str]=manifest_dict[target]["extra_preproc_defflag"]
			if "extra_preproc_defflag_per_file" in manifest_dict[target]:
				self.cflags_per_file:dict=manifest_dict[target]["extra_preproc_defflag_per_file"]
			if "extra_preproc_asm_defflag" in manifest_dict[target]:
				self.aflags:list[str]=manifest_dict[target]["extra_preproc_asm_defflag"]
			# Additional Platform-specific include headers
			if "platform" in manifest_dict[target]:
				if manifest_dict[target]["platform"]=="windrv":
					self.c_includes+=compiler.wdk_incpath
				elif manifest_dict[target]["platform"]=="user":
					self.c_includes+=compiler.sdk_incpath
			if "platform_per_file" in manifest_dict[target]:
				self.platform_per_file:dict=manifest_dict[target]["platform_per_file"]
			if verbose:
				print(self.c_sources)
		else:
			print("Specified target does not exist!")

class executable_unit:
	def __init__(self,manifest:manifest_unit,compiler:compiler_unit,output_dict:dict,optimize:bool=False):
		file_type_dict:dict={"win-drv":"sys","win-stalib":"lib","uefi-app":"efi","uefi-rtdrv":"efi"}
		self.optimize:bool=optimize
		self._internal_dict:dict={}
		self._internal_dict["opt"]="fre" if self.optimize else "chk"
		self.file_name:str=output_dict["file_name"]
		self.file_type:str=output_dict["file_type"]
		self.output_dir:str=output_dict["output_dir"]
		self.output_dir=self.output_dir.format(**self._internal_dict)
		self.libraries:list[str]=[]
		self.external_libraries:list[str]=[]
		if "libraries" in output_dict:
			self.libraries+=output_dict["libraries"]
		if "external_libraries" in output_dict:
			self.external_libraries+=output_dict["external_libraries"]
		try:
			self.entry_point:str=output_dict["entry_point"]
		except KeyError:
			self.entry_point:str=None
		self.intermediate_dir:str=output_dict["intermediate_dir"]
		self.intermediate_dir=self.intermediate_dir.format(**self._internal_dict)
		self._internal_dict["output_base"]=self.output_dir
		self._internal_dict["target_name"]=self.file_name
		self._internal_dict["extension"]=file_type_dict[self.file_type]
		self.compiler:compiler_unit=compiler
		self.manifest:manifest_unit=manifest
		self.objects:list[object_unit]=[]
		self.initialize_objects(manifest)
		self.cmd:list[str]=[]
		file_type_dict:dict={"C":"obj","Asm":"obj","Res":"res"}
		if compiler.family=="msvc":
			if self.file_type=="win-stalib":
				self.cmd.append("lib")
				for obj in self.objects:
					out_fn="{}{}{}.{}".format(self.intermediate_dir,os.sep,obj.target_name,file_type_dict[obj.file_type])
					self.cmd.append(out_fn)
				self.cmd+=compiler.lib_flags
				self.cmd.append(compiler.binary_output.format(**self._internal_dict))
			else:
				self.cmd.append("link")
				for obj in self.objects:
					out_fn="{}{}{}.{}".format(self.intermediate_dir,os.sep,obj.target_name,file_type_dict[obj.file_type])
					self.cmd.append(out_fn)
				for lib in self.libraries:
					lib_fn="{}{}{}".format(self.output_dir,os.sep,lib)
					self.cmd.append(lib_fn)
				for libpath in compiler.wdk_libpath:
					self.cmd.append(compiler.libpath_flag.format(libpath))
				self.cmd+=self.external_libraries
				self.cmd+=compiler.linker_flags
				subsystem_dict:dict={"win-drv":"NATIVE","uefi-app":"EFI_APPLICATION","uefi-rtdrv":"EFI_RUNTIME_DRIVER"}
				self.cmd.append("/SUBSYSTEM:{}".format(subsystem_dict[self.file_type]))
				self.cmd.append(compiler.symbol_output.format(**self._internal_dict))
				self.cmd.append(compiler.binary_output.format(**self._internal_dict))
				self.cmd.append(compiler.entry_point.format(self.entry_point))

	def initialize_objects(self,manifest:manifest_unit)->None:
		# C Sources
		for src in manifest.c_sources:
			cflags=manifest.cflags.copy()
			includes=manifest.c_includes.copy()
			src_fn=os.path.split(src)[-1]
			if src_fn in manifest.cflags_per_file:
				cflags+=manifest.cflags_per_file[src_fn]
			if src_fn in manifest.platform_per_file:
				if manifest.platform_per_file[src_fn]=="windrv":
					includes+=self.compiler.wdk_incpath
				elif manifest.platform_per_file[src_fn]=="user":
					includes+=self.compiler.sdk_incpath
				else:
					print("Unknown platform-per-file: {}".format(manifest.platform_per_file[src_fn]))
			self.objects.append(object_unit(src,self.compiler,"C",self.intermediate_dir,includes,cflags))
		# Assembly Sources
		for src in manifest.asm_sources:
			aflags=manifest.aflags.copy()
			src_fn=os.path.split(src)[-1]
			self.objects.append(object_unit(src,self.compiler,"Asm",self.intermediate_dir,manifest.c_includes,aflags))
		# Resource Sources
		for src in manifest.rc_sources:
			cflags=manifest.cflags.copy()
			src_fn=os.path.split(src)[-1]
			if src_fn in manifest.platform_per_file:
				if manifest.platform_per_file[src_fn]=="windrv":
					includes+=self.compiler.wdk_incpath
				elif manifest.platform_per_file[src_fn]=="user":
					includes+=self.compiler.sdk_incpath
				else:
					print("Unknown platform-per-file: {}".format(manifest.platform_per_file[src_fn]))
			self.objects.append(object_unit(src,self.compiler,"Res",self.intermediate_dir,includes,cflags))
		# Manifest has a tree structure.
		for subm in manifest.subordinate_manifests:
			self.initialize_objects(subm)

	def link(self):
		global verbose
		if verbose:
			print(self.cmd)
		self.process=subprocess.Popen(self.cmd,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
		stdout_bytes,stderr_bytes=self.process.communicate()
		self.stdout_text=stdout_bytes.decode('latin-1')
		self.stderr_text=stderr_bytes.decode('latin-1')
		print(self.stdout_text,end='')
		print(self.stderr_text,end='')

	def serial_build(self):
		for obj in self.objects:
			obj.build()
			obj.wait()
			print(obj.stdout_text,end='')
		self.link()

	def parallel_build(self):
		for obj in self.objects:
			obj.build()
		for obj in self.objects:
			obj.wait()
			print(obj.stdout_text,end='')
		self.link()

def main()->None:
	global verbose
	i=1
	parallel_compilation:bool=False
	platform:str="win7x64"
	target:str="hypervisor"
	while i<len(sys.argv):
		if sys.argv[i]=="/j":
			parallel_compilation=True
		elif sys.argv[i]=="/v":
			verbose=True
		elif sys.argv[i]=="/platform":
			i+=1
			platform=sys.argv[i]
		elif sys.argv[i]=="/target":
			i+=1
			target=sys.argv[i]
		else:
			print("Ignoring unknown argument: {}!".format(sys.argv[i]))
		i+=1
	# Load compiler definitions.
	cc_dict_fn=open("build/compilers.json")
	cc_dict_str:str=cc_dict_fn.read()
	cc_dict_fn.close()
	cc_dict:dict=json.loads(cc_dict_str)
	# Load preset definitions.
	ps_dict_fn=open("build/presets.json")
	ps_dict_str:str=ps_dict_fn.read()
	ps_dict_fn.close()
	ps_dict:dict=json.loads(ps_dict_str)
	# Check if platform preset is valid
	if not platform in ps_dict:
		print("Unknown platform!")
		return
	preset=ps_dict[platform]
	compiler=compiler_unit(cc_dict[preset["compiler"]],preset["platform"])
	# Load manifest.
	manifest=manifest_unit("build.json",target,compiler,platform)
	# Create executable.
	outputs_dict:dict=preset["outputs"]
	if target in outputs_dict:
		out_dict=outputs_dict[target]
		executable=executable_unit(manifest,compiler,out_dict)
		if parallel_compilation:
			executable.parallel_build()
		else:
			executable.serial_build()
	else:
		print("Target {} is unknown!".format(target))

if __name__=="__main__":
	if platform.system()!="Windows":
		print("{} is unsupported!".format(platform.system()))
		exit()
	t1:float=time.time()
	main()
	t2:float=time.time()
	print("{} seconds spent in compilation!".format(t2-t1))