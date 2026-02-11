#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import hashlib
import argparse

import os, sys, struct, zlib, datetime

from pprint import pprint

def sizeof_fmt(num, suffix='B'):
    for unit in ['','Ki','Mi','Gi','Ti','Pi','Ei','Zi']:
        if abs(num) < 1024.0:
            return "%3.1f %s%s" % (num, unit, suffix)
        num /= 1024.0
    return "%.1f %s%s" % (num, 'Yi', suffix)
    
def list_files( startpath, alias ):
	
	file_list = []
	
	st_mtime = 0
	
	for root, dirs, files in os.walk(startpath):
    
		root = root.replace(startpath, '') 
    
		if root.endswith( "/" ):
			root = root[:-1]
			
		for tmp in files:
			
			f = root + "/" + tmp
			
			stat = os.stat( startpath + f )
			if st_mtime < stat.st_mtime:
				st_mtime = stat.st_mtime
				
			file_list.append( {"PATH" : f , "SIZE" : stat.st_size } )
	
	return [ int( st_mtime ), file_list ]

def write_uint32( f, value ):
	#print( int ( value ) )
	data = struct.pack( 'I', int ( value ) )
	for a in data:
		if sys.version_info[0] == 2:
			f.write( "" + str( ord(a) ) + "," )
		else:
			f.write( "" + str( a ) + "," )

def write_uint64( f, value ):
	#print( int ( value ) )
	data = struct.pack( 'L', int ( value ) )
	#pprint(data)
	for a in data:
		if sys.version_info[0] == 2:
			f.write( "" + str( ord(a) ) + "," )		
		else:
			f.write( "" + str( a ) + "," )		
		

def write_string( f, value ):
	
	write_uint32( f, len( value ) )
	
	for a in value:
		f.write( "'" + str( a ) + "'," )
	
	f.write( "'\\0'," )

def bytes_from_file(filename):
	with open(filename, "rb") as f:
		while True:
			byte = f.read(1)
			if not byte:
				break
			yield(ord(byte))


def check_template( f,  path , f_data ):
	"""
	schreibt den wert direkt ins file f
	"""
	f2 = open( path + f_data["PATH"], "rb")
	data = f2.read(20)
	f2.close()
	
	template = 0
	if data.startswith(b"TEMPLATE_V1"):
		template=1
		
	if f_data["PATH"].endswith(".inc"):
		template=1
	
	write_uint32( f, template )
	
	return template
	

def write_uncompressed_file( stats, f, path , f_data ):
	
	f2 = open( path + f_data["PATH"], "rb")
	data = f2.read()
	f2.close()
	
	h = hashlib.md5()
	h.update( data )
	etag = h.hexdigest().upper()
	
	write_uint32( f, 0 ) # nicht komprimiert
	write_string( f, f_data["PATH"] )
	write_string( f, etag )
	check_template( f, path, f_data )	# auf template prüfen
	write_uint32( f, f_data["SIZE"] )
	
	stats["compressed"] += f_data["SIZE"]
	stats["uncompressed"] += f_data["SIZE"]
	
	if sys.version_info[0] == 2:
		for b in data:
			f.write( "" + str(ord(b))  + "," )
	else:
		for b in bytes_from_file( path + f_data["PATH"]):
			f.write( "" + str( b ) + "," )
	f.write( "'\\0'," )

def compress_data( data ):
	
	strats = [ 
		{ "strategy": zlib.Z_DEFAULT_STRATEGY,	"size": 0, "data": None},
		{ "strategy": zlib.Z_FILTERED, 			"size": 0, "data": None},
		{ "strategy": zlib.Z_HUFFMAN_ONLY, 		"size": 0, "data": None},
	]
	outs = []
	
	for strat in strats:
		if sys.version_info[0] == 2:
			compress = zlib.compressobj( 9, zlib.DEFLATED, -15 ,9, strat["strategy"] )
		else:
			compress = zlib.compressobj( level=9, method=zlib.DEFLATED, wbits=-15 ,memLevel=9, strategy=strat["strategy"] )
		deflated = compress.compress(data)
		deflated += compress.flush()
		strat["size"] = len( deflated )
		strat["data"] = deflated
	
	smalest = { "strategy": -1, 	"size": len( data ), "data": data}
	for strat in strats:
		if smalest["size"] > strat["size"]:
			smalest = strat
	
	return smalest
	

def write_compressed_file( stats, f, path, f_data ):
				
	with open( path + f_data["PATH"], 'rb') as f_in:
		s_in = f_in.read()
		
		# etag generieren
		h = hashlib.md5()
		h.update( s_in )
		etag = h.hexdigest().upper()
		
		compressed = compress_data( s_in )
		
		
		if compressed["size"] < f_data["SIZE"]:
			
			write_uint32( f, 2 )  # komprimiert mit deflate
			write_string( f, f_data["PATH"] )
			write_string( f, etag )
			tp = check_template( f, path, f_data )	# auf template prüfen
			write_uint32( f, f_data["SIZE"] )
			
			#text = "compressing ( " 
			#text += str ( int ( ( compressed["size"] / f_data["SIZE"] ) * 100 ) ) + " % " 
			#text += " )  : " + f_data["PATH"] 
			#if  tp == 1 :
			#	text += "  ( template )"
			#sys.stdout.write( text + "\r")
			
			write_uint32( f, compressed["size"])
			#pprint( type( compressed["data"] ))
			for b in compressed["data"]:
				if sys.version_info[0] == 2:
					f.write( "" + str( ord( b ))  + "," )
				else:
					f.write( "" + str( b ) + "," )
			
			stats["compressed"] += compressed["size"]
			stats["uncompressed"] += f_data["SIZE"]
				
		else:
			
			write_uncompressed_file( stats, f, path, f_data )
			
def gen_c_file( path, alias, outname ):
	
	if not os.path.exists( path ):
		print( "path not found : " + path )
		sys.exit(1)


	liste = list_files ( path, alias )
	
	info = { "ALIAS" : alias , "PATH": path , "TIME" : liste[0], "FILES" : liste[1] }
	
	
	if not outname.endswith(".c"):
		outname = outname + ".c"
	
	print("\nwriting : " + outname )
	print("  Alias : " + info["ALIAS"] )
	#print("  Time  : " + str( int( info["TIME"] ) ))
	print("  Time  : " +  datetime.datetime.utcfromtimestamp(int( info["TIME"] )).strftime('%Y-%m-%d %H:%M:%S') )
	print("  Files : " + str (len (info["FILES"] ) ) )
	
	file_site_stats = {"compressed":0,"uncompressed":0}
	
	with open(  outname, "w" ) as f:
	
		f.write("\n")
		
		f.write("unsigned char data_" + alias.replace("/","_") + "[] = {" )
		
		write_string( f, info["ALIAS"] )
		write_uint64( f, info["TIME"] )
		
		write_uint32( f, len( info["FILES"] ) )
		for f_data in info["FILES"]:
			if f_data["PATH"].lower().endswith(".jpg"): 
				write_uncompressed_file( file_site_stats, f, info["PATH"],  f_data )
				continue
			write_compressed_file( file_site_stats, f, info["PATH"],  f_data )
			
			
			
		f.write("0};\n")
		f.write("\n")
	
	comp_ratio = ( file_site_stats["compressed"] / file_site_stats["uncompressed"] ) * 100
	text = "  Size  : %.1f %%" % ( comp_ratio )
	text += " ( " + sizeof_fmt( file_site_stats["compressed"] )
	text += " / " + sizeof_fmt( file_site_stats["uncompressed"] ) +  " )  \n"
	
	print(text)
	
	#st = os.stat(outname)
	#print("  c code size   : " + sizeof_fmt ( st.st_size ) )
	
	#os.system("gcc -O0 -c " + outname + " -o /tmp/comp_test ")
	#st = os.stat("/tmp/comp_test")
	#print("  c object size : " + sizeof_fmt( st.st_size ) )
	

parser = argparse.ArgumentParser()
parser.add_argument("--dir", help="input directory", required=True)
parser.add_argument("--alias", help="directory name in urls", required=True)
parser.add_argument("--out", help="output file name", required=True)
args = parser.parse_args()
    
if args.out.endswith(".c"):
	args.out = args.out.replace(".c","")
	
gen_c_file( args.dir , args.alias, args.out )

#gen_c_file( "../testSite/img/" , "img" )

