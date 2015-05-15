# ins
integrated naming service (based on raft)

# build
* comake2 -UB
* comake2
* make

# try
	   cd sandbox
	  ./start_all.sh
	  ./ins_shell.sh
	
	  galaxy ins>  
	    show [ show cluster ]
	    put (key) (value) [ update the data ] 
	    get (key) [read the data by key ]
	    delete (key) [remove the data by key]
  
