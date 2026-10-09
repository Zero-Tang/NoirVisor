import os
import sys

if __name__=="__main__":
	# The command of file deletion is not unified among Windows and Linux.
	# So here's a simple python script to do the job.
	# This is required because osslsigncode cannot overwrite files.
	files=sys.argv[1:]
	for f in files:
		try:
			os.remove(f)
		except FileNotFoundError:
			pass
		except:
			print("Failed to remove file {}!".format(f))