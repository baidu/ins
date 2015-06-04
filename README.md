# ins
integrated naming service (based on raft)

[![Build Status](https://travis-ci.org/fxsjy/ins.svg?branch=master)](https://travis-ci.org/fxsjy/ins)

# build for Baiduer
* comake2 -UB
* comake2
* make

# build default
* read .travis.yml and install the depends libs
* make && make cp

# try it

	cd sandbox
	./start_all.sh
	./ins_shell.sh
		
	galaxy ins> 
	  show [ show cluster ]
	  put (key) (value) [ update the data ] 
	  get (key) [read the data by key ]
	  delete (key) [remove the data by key]
	  scan (start key) (end key)
  
# performance (multiple clients， 5 cluster-nodes in LAN)
* 55200 read per second
* 5300 write per second
