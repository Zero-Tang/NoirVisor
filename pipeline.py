#!/usr/bin/python3
import json
import os
import subprocess
import threading

class PipelineInstruction:
	def __init__(self,parent,name:str,raw:dict,opt:bool):
		self.name=name
		self.cmd:list[str]=raw["cmd"]
		self.dependency_names:list[str]=raw["dependencies"] if "dependencies" in raw else []
		self.dependencies:list[PipelineInstruction]=[]
		self.variables:dict[str,str]=raw["var"] if "var" in raw else dict()
		self.parent:Pipeline=parent
		self.proc:subprocess.Popen|None=None
		self.optimizer_enabled=opt
		self.progressive:bool=raw["progressive"] if "progressive" in raw else False

	def add_dependency(self,other):
		self.dependencies.append(other)
	
	def worker(self):
		# The internal dictionary will help format the command strings.
		internal_dict={"instruction":self.name}|self.parent.global_variable|self.variables
		# Load instruction configurations.
		cmd=self.cmd
		if self.cmd[0] in self.parent.common_flags:
			cmd+=self.parent.common_flags[cmd[0]]
		if self.optimizer_enabled:
			if cmd[0] in self.parent.optimizer_flags:
				cmd+=self.parent.optimizer_flags[cmd[0]]
		for i in range(len(cmd)):
			old=cmd[i]
			new=cmd[i].format(**internal_dict)
			while old!=new:
				old=new
				new=old.format(**internal_dict)
			cmd[i]=new
		# print(cmd)
		# Wait for all dependent instructions to finish.
		for dep in self.dependencies:
			dep.wait()
		try:
			if self.progressive:
				# Progressive instructions will gain exclusive access to the console
				# because they print their job's progress on the console.
				# However, progressive instructions block each other,
				# so you should reduce the number of progressive instructions.
				self.parent.console_lock.acquire()
				proc=subprocess.run(cmd)
				self.parent.console_lock.release()
				self.return_code=proc.returncode
			else:
				# Non-progressive instructions won't be blocked by progressive instructions.
				self.proc=subprocess.Popen(cmd,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
				stdout_bytes,stderr_bytes=self.proc.communicate()
				self.stdout_text=stdout_bytes.decode()
				self.stderr_text=stderr_bytes.decode()
				self.parent.console_lock.acquire()
				print("{}".format(self.stdout_text),end='')
				print("{}".format(self.stderr_text),end='')
				self.parent.console_lock.release()
				self.return_code=self.proc.returncode
		except:
			self.proc=None
			print("[{}] failed to run!".format(self.name))

	def run(self):
		self.thread=threading.Thread(target=self.worker)
		self.thread.start()
	
	def wait(self)->int|None:
		self.thread.join()
		return self.return_code

class Pipeline:
	def __init__(self,config:str,opt:bool=False,outdir:str|None=None,extra_vars:dict[str,str]=dict()):
		f=open(config,'r')
		self._raw=json.load(f)
		f.close()
		self.console_lock=threading.Lock()
		# Load the pipeline configuration.
		self.extra_env:dict[str,str]=self._raw["extra_env"] if "extra_env" in self._raw else dict()
		self.common_flags:dict[str,list[str]]=self._raw["common_flags"] if "common_flags" in self._raw else dict()
		self.optimizer_flags:dict[str,list[str]]=self._raw["opt_flags"] if "opt_flags" in self._raw else dict()
		self.internal_variables:dict[str,str]=self._raw["internal_var"] if "internal_var" in self._raw else dict()
		self.global_variable:dict[str,str]={
			"objpath":".\\bin\\{}\\Intermediate".format(self.internal_variables["outdir_"+("rel" if opt else "dev")] if outdir is None else outdir),
			"outdir":self.internal_variables["outdir_"+("rel" if opt else "dev")] if outdir is None else outdir,
			"cargo_preset":"release" if opt else "debug",
			"cd":os.getcwd()}|os.environ|extra_vars
		self.instructions:dict[str,PipelineInstruction]=dict()
		# Load all instructions into the pipeline.
		for i_name in self._raw["instructions"]:
			self.instructions[i_name]=PipelineInstruction(self,i_name,self._raw["instructions"][i_name],opt)
		# Resolve instruction dependencies.
		for i_name in self.instructions:
			for d_name in self.instructions[i_name].dependency_names:
				self.instructions[i_name].add_dependency(self.instructions[d_name])
	
	def run(self):
		# Append extra environment variables.
		old_environ=os.environ.copy()
		for env_key in self.extra_env:
			env_value=self.extra_env[env_key].format(**self.global_variable)
			os.environ[env_key]=env_value
		# Run the pipeline.
		for i_name in self.instructions:
			instr=self.instructions[i_name]
			instr.run()
		# Wait for the pipeline to finish.
		for i_name in self.instructions:
			instr=self.instructions[i_name]
			instr.wait()
		# Restore environment variables.
		os.environ=old_environ