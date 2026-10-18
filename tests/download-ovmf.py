import os
import requests
import sys
import zipfile

if __name__=="__main__":
	if not (os.path.exists("ovmf-code.fd") and os.path.exists("ovmf-vars.fd")):
		print("Missing OVMF Binary files...")
		if not os.path.exists("ovmf.zip"):
			print("Missing OVMF Archive... Downloading from GitHub...")
			if len(sys.argv)>1 and sys.argv[1]=="bochs":
				url="https://github.com/user-attachments/files/33619214/ovmf.zip"
			else:
				url="https://github.com/user-attachments/files/21815836/ovmf.zip"
			try:
				resp=requests.get(url)
				f=open("ovmf.zip",'wb')
				f.write(resp.content)
				f.close()
			except:
				print("Failed to download from GitHub! You may try to manually download OVMF from the following link:")
				print(url)
				print("Then extract the \"ovmf-code.fd\" and \"ovmf-vars.fd\" files to the \"tests\" directory!")
		if os.path.exists("ovmf.zip"):
			print("Extracting OVMF...")
			z=zipfile.ZipFile("ovmf.zip")
			z.extract("ovmf-code.fd")
			z.extract("ovmf-vars.fd")
			z.close()
			print("Completed!")