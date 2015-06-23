# INS
Ivy Naming Service (based on Raft)

[![Build Status](https://travis-ci.org/fxsjy/ins.svg?branch=master)](https://travis-ci.org/fxsjy/ins)

# build for Baiduer
* comake2 -UB
* comake2
* make

# build default
* read .travis.yml and install the depends libs
* make && make install

# try it

	cd sandbox
	./start_all.sh
	./ins_shell.sh
		
	galaxy ins> help
	  show [ show cluster ]
	  put (key) (value) [ update the data ] 
	  get (key) [read the data by key ]
	  delete (key) [remove the data by key]
	  scan (start-key) (end-key) [scan from start-key to end-key(excluded)]
	  watch (key) [event will be triggered once value changed or deleted]
	  lock (key) [lock on specific key]
	  enter quit to exit shell
  
# performance (multiple clients， 5 cluster-nodes in LAN)
* 55200 read per second
* 5300 write per second
