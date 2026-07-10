#!/usr/bin/python
# -*- coding: utf-8 -*-

# Copyright (c) 2023 Marius Gräfe
# Script to generate version.h from git output


import re
import sys
import subprocess
import os

productName = 'ts3sb'; # Set product name here

outFileVersion = 'src/version/version.h'
outFilePackage = 'deploy/package.ini'

def get_version_string():
	try:
		return subprocess.check_output(
			['git', 'describe', '--tags'], stderr=subprocess.DEVNULL
		).decode().strip()
	except (subprocess.CalledProcessError, FileNotFoundError):
		# No tags in this clone (common on forks / shallow CI checkouts).
		# Still produce a parseable version so the build can proceed.
		try:
			commit = subprocess.check_output(
				['git', 'rev-parse', '--short', 'HEAD'], stderr=subprocess.DEVNULL
			).decode().strip()
			return 'v0.0.0-untagged-' + commit
		except (subprocess.CalledProcessError, FileNotFoundError):
			return 'v0.0.0-untagged'


def main():
	versionStr = get_version_string()
	print('Using version: ' + versionStr)

	checkFile = 'release/git-state.txt'
	if os.path.isfile(checkFile):
		with open(checkFile, 'r') as fh:
			content = fh.read()
			if content == versionStr and os.path.isfile(outFileVersion) and os.path.isfile(outFilePackage):
				print('Output files exist and git tag did not change, not running version update')
				return 0

	print('Git change or missing file detected, re-creating files...')
	try:
		os.makedirs(os.path.dirname(checkFile), exist_ok=True)
		fh = open(checkFile, 'w')
		fh.write(versionStr)
	except:
		print('Unable to create git check file, write protected?')

	pattern = 'v?([0-9]+)\\.([0-9]+)\\.([0-9]+)'

	reObj = re.search(pattern, versionStr)
	if reObj == None:
		print('Pattern not found')
		return 1

	groups = list(reObj.groups())

	productNameCap = productName.upper()
	
	# Generate version.h
	sh = \
	'//CHANGES WILL BE OVERWRITTEN BY version.py\n\n' \
	'#ifndef ' + productName + '_all__version_H__\n' \
	'#define ' + productName + '_all__version_H__\n\n' \
	'#define ' + productNameCap + '_VERSION ' + ','.join(groups) + ',0\n' + \
	'#define ' + productNameCap + '_VERSION_S "' + versionStr + '"\n' + \
	'#define ' + productNameCap + '_VERSION_MAJOR ' + groups[0] + '\n' + \
	'#define ' + productNameCap + '_VERSION_MINOR ' + groups[1] + '\n' + \
	'#define ' + productNameCap + '_VERSION_REVISION ' + groups[2] + '\n' + \
	'#define ' + productNameCap + '_VERSION_BUILD ' + str(int(groups[0]) * 10000 + int(groups[1]) * 100 + int(groups[2])) + '\n' + \
	'\n#endif\n'
	
	os.makedirs(os.path.dirname(outFileVersion), exist_ok=True)
	with open(outFileVersion, 'w') as fh:
		fh.write(sh)
		fh.close()

	# Generate package.ini
	with open('src/package.ini.in', 'r') as fh:
		txt = fh.read().replace('@version@', versionStr)
	os.makedirs(os.path.dirname(outFilePackage), exist_ok=True)
	with open(outFilePackage, 'w') as fh:
		fh.write(txt)

	return 0


sys.exit(main())
