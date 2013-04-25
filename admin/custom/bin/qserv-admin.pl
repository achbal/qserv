#!/usr/bin/perl -w

use strict;
use Getopt::Long;
use Cwd;

Getopt::Long::config('bundling_override');
my %opts = ();
GetOptions( \%opts, 
	"debug",
	"help|h",
	"set",
	"status|s",
	"stripes=i",
	"stop",
	"start",
	"substripes=i",
	"load",
        "mono-node",
	"delete-data",
	"source=s",
	"table=s",
	"stripedir|sdir=s",
	"mysql-proxy-port=s",
	"dbpass=s",
	"partition",
	"test"
);
usage(1) if ($Getopt::Long::error); 
usage(0) if ($opts{'help'});

my $debug = $opts{'debug'} || 0;

# WARNING:  %(...) below are template variables !
# It  would  be  better  to  read  config files  instead  to  have  it
# hard-coded here. see https://dev.lsstcorp.org/trac/ticket/2566

my $install_dir = "%(QSERV_BASE_DIR)s";
my $mysql_proxy_port = "%(MYSQL_PROXY_PORT)s" || $opts{'mysql-proxy-port'} || 4040;
my $cluster_type = $opts{'mono-node'} || "mono-node" ;

print "Using $install_dir install.\n" if( $debug );

#mysql variables
my $mysqld_sock = "$install_dir/var/lib/mysql/mysql.sock";

if( $opts{'status'} ) {
	print "Checking on the status.\n" if( $debug );

	unless( $opts{'dbpass'} ) {
		print "Error: you need to specify the mysql root password with the --dbpass option.\n";
		exit(1);
	}

	if( check_mysqld( $opts{'dbpass'} ) ) {
		print "Mysql server up and running.\n";
	} else {
		print "Mysql server not running.\n";
	}

	if( check_proxy() ) {
		print "Mysql proxy up and running.\n";
	} else {
		print "Mysql proxy not running.\n";
	}

	if( check_xrootd() ) {
		print "Xrootd server up and running.\n";
	} else {
		print "Xrootd server not running.\n";
	}

	if( check_qserv() ) {
		print "Qserv server up and running.\n";
	} else {
		print "Qserv server not running.\n";
	}

} elsif( $opts{'stop'} ) {
	
	print "Stopping mysql-proxy\n";
	stop_proxy();
	print "Stopping xrootd\n";
	stop_xrootd();
	print "Stopping mysqld\n"; 
	stop_mysqld();
	print "Stopping qserv\n"; 
	stop_qserv();
	
} elsif( $opts{'start'} ) {

	start_proxy();
	start_mysqld();
        start_qms();        
        # xrootd will launch mysql queries at startup
        sleep(2);
	start_xrootd();
	start_qserv();
	
} elsif( $opts{'partition'} ) {

	#need to partition raw data for loading into qserv.
	unless( $opts{'source'} ) {
		print "Error: you need to set the path to the source data with the --source option.\n";
		exit(1);
	}
	unless( $opts{'table'} ) {
		print "Error: you need to specify the table name for the source data with the --table option.\n";
		exit(1);
	}
	unless( $opts{'stripedir'} ) {
		print "Error: you need to specify the stripe dir path with the --stripedir option.\n";
		exit(1);
	}
	
	partition_data( $opts{'source'}, $opts{'stripedir'}, $opts{'table'} );
	
} elsif( $opts{'load'} ) {

	#need to partition raw data for loading into qserv.
	unless( $opts{'source'} ) {
		print "Error: you need to set the path to the source data with the --source option.\n";
		exit(1);
	}
	unless( $opts{'table'} ) {
		print "Error: you need to specify the table name for the source data with the --table option.\n";
		exit(1);
	}
	unless( $opts{'stripedir'} ) {
		print "Error: you need to specify the stripe dir path with the --stripedir option.\n";
		exit(1);
	}
	unless( $opts{'dbpass'} ) {
		print "Error: you need to specify the mysql root password with the --dbpass option.\n";
		exit(1);
	}

	#need to load data into qserv

	## TODO performs some checking before starting mysqld (already started)
	## and idem for stopping
	load_data( $opts{'source'}, $opts{'stripedir'}, $opts{'table'}, $opts{'dbpass'} );
	
} elsif( $opts{'set'} ) {

	unless( $opts{'stripes'} && $opts{'stripes'} =~ /\d+/ ) {
		print "Error: sorry you need to set the stripes you are using with the 'stripes' option\n";
		exit(1);
	}
	unless( $opts{'substripes'} && $opts{'substripes'} =~ /\d+/ ) {
		print "Error: sorry you need to set the substripes you are using with the 'substripes' option\n";
		exit(1);
	}
	
	set_stripes( $opts{'stripes'}, $opts{'substripes'} );

} elsif( $opts{'delete-data'} ) {

	unless( $opts{'dbpass'} ) {
		print "Error: you need to specify the mysql root password with the --dbpass option.\n";
		exit(1);
	}

	# deleting data from qserv
	delete_data( $opts{'source'}, $opts{'stripedir'}, $opts{'table'}, $opts{'dbpass'} );

}

exit( 0 );

#############################################################

#Check the sql server status, if it is up or down by using 
#an sql command for input.
sub check_sql_server {
	my( $command ) = @_;

	print "Testing sql with command $command\n" if( $debug );
	
	my $testsql = "/tmp/tmp.sql";
	create_test_sql( $testsql );
	
	#try through the proxy to see if it can talk to mysql server.
	my @reply = run_command("$command < $testsql 2>&1");
	
	print "@reply\n" if( $debug );
	
	if( $reply[0] =~ /Database/ ) {
		return 1;
	} else {
		return 0;
	}
	
	unlink "$testsql";
	
}

#Create a test sql command to test the sql server.
sub create_test_sql {
	my( $testsql ) = @_;
	
	#create tmp sql file
	open TMPFILE, ">$testsql";
	print TMPFILE "show databases;\n";
	close TMPFILE;
	
}

#check the mysql proxy use.
sub check_proxy {

	return check_sql_server( "mysql --port=$mysql_proxy_port --protocol=TCP" );

}

#check the mysql server status.
sub check_mysqld {

	my( $dbpass ) = @_;
	if( -e "$mysqld_sock" ) {
		return check_sql_server( "mysql -S $mysqld_sock -u root -p$dbpass" );
	} else {
		return 0;
	}
}

#check the xrootd process.
sub check_xrootd {
	
	if( check_ps( "xrootd -c" ) && check_ps( "cmsd -c" ) ) {
		return 1;
	} else {
		return 0;
	}

}

#check the qserv process.
sub check_qserv {
	
	return check_ps( "startQserv" );

}

#Check the existence of a process in the process table by a test 
#string in the command line used.
sub check_ps {
	my( $test_string ) = @_;
	print "Check_ps : $test_string\n";
	
	my @reply = run_command("ps x");
	print "Reply : @reply\n";
	my @stuff = grep /$test_string/, @reply;
	print "Catched stuff : @stuff\n-----\n";
	
	if( @stuff ) {
		my( $pid ) = $stuff[0] =~ /^\s*(\d+) /;
		return $pid;
	} else {
		return 0;
	}
}

#Stop the qserv process
sub stop_qserv {
    killpid("$install_dir/var/run/qserv.pid");
}

#stop the xrootd process
sub stop_xrootd {
    killpid("$install_dir/var/run/xrootd.pid");
    if ($cluster_type ne "mono-node") {
        killpid("$install_dir/var/run/cmsd.pid");
    }
}

#stop the mysql server
sub stop_mysqld {
  my( $dbpass ) = $opts{'dbpass'};
  run_command("$install_dir/bin/mysqladmin -S $install_dir/var/lib/mysql/mysql.sock -u root -p$dbpass shutdown");
}

#stop the mysql proxy
sub stop_proxy {
    killpid("$install_dir/var/run/mysql-proxy.pid");
}

# Kill a process based on a PID file
sub killpid {
    my( $pidfile ) = @_;
    if (-e $pidfile) {
        print "Killing process pids from $pidfile \n";
        open FILE, $pidfile;
        while (<FILE>) {
            kill 9, $_;
        }
        unlink $pidfile
    } else {
        print "killpid: Non existing PID file $pidfile \n";
    }
}

#Stop a process given an id
sub stop_ps {
	my( $pid ) = @_;
	
	#print "pid to stop -- $pid\n";
	if( $opts{'test'} ) {
		print "I would now kill process $pid.\n";
	} else {
		`kill $pid`;
	}
}

#get the pid from a file
sub get_pid {
	my( $filename ) = @_;
	
	open PIDFILE, "<$filename" 
		or warn "Sorry, can't open $filename\n";
	my $pid = <PIDFILE>;
	chomp $pid;
	close PIDFILE;
	
	return $pid;
}

sub start_proxy {

	system("$install_dir/start_mysqlproxy");

}

sub start_mysqld {

	system("$install_dir/bin/mysqld_safe &");

}

sub start_qms {

	system("$install_dir/start_qms &");

}

sub start_qserv {

	system("$install_dir/start_qserv");

}

sub start_xrootd {

	system("$install_dir/start_xrootd");
	
}

sub set_stripes {
	my( $stripes, $substripes ) = @_;
	
	my $reply = `grep stripes $install_dir/etc/local.qserv.cnf`;
	
	if( $reply =~ /stripes\s*=\s*\d+/ ) {
		print "Error: sorry, you have already set the stripes to use for the data.\n";
		print "    please edit the local.qserv.cnf and remove the stripes value if you really want to change this.\n";
		return;
	}

	`perl -pi -e 's,^substripes\\s*=,substripes = $substripes,' $install_dir/etc/local.qserv.cnf`;
	`perl -pi -e 's,^stripes\\s*=,stripes = $stripes,' $install_dir/etc/local.qserv.cnf`;

}

#Partition the pt11 example data for use into chunks.  This is the example
#use of partitioning, and this should be more flexible to create different
#amounts of chunks, but works for now.
sub partition_data {
	my( $source_dir, $output_dir, $tablename ) = @_;
	
	my $stripes;
	my $substripes;
	
	my $reply = `grep stripes $install_dir/etc/local.qserv.cnf`;
	if( $reply =~ /^stripes\s*=\s*(\d+)/ ) {
		if( $opts{'stripes'} ) {
			unless( $opts{'stripes'} == $1 ) {
				print "Warning: you used $1 for stipes before.  Please check which stripes value you wish to use.\n";
				return;
			}
		}
		$stripes = $1;
	} else {
		if( $opts{'stripes'} ) {
			$stripes = $opts{'stripes'};
		} else {
			print "Error: sorry you need to set the stripes value before partitioning.\n";
			return;
		}
	}
	
	$reply = `grep substripes $install_dir/etc/local.qserv.cnf`;
	if( $reply =~ /substripes\s*=\s*(\d+)/ ) {
		if( $opts{'substripes'} ) {
			unless( $opts{'substripes'} == $1 ) {
				print "Warning: you used $1 for substipes before.  Please check which substripes value you wish to use.\n";
				return;
			}
		}
		$stripes = $1;
	} else {
		if( $opts{'substripes'} ) {
			$stripes = $opts{'substripes'};
		} else {
			print "Error: sorry you need to set the substripes value before partitioning.\n";
			return;
		}
	}
	
	my( $dataname ) = $source_dir =~ m!/([^/]+)$!;
	
	if( -d "$output_dir" ) {
		chdir "$output_dir";
	} else {
		print "Error: the stripe dir $output_dir doesn't exist.\n";
	}

	#need to have the various steps to partition data
	my $command = "$install_dir/bin/python $install_dir/qserv/master/examples/partition.py ".
		"-P$tablename -t 2 -p 4 $source_dir/${tablename}.txt -S $stripes -s $substripes";
	if( $opts{'test'} ) {
		print "$command\n";
	} else {
		run_command("$command");
	}
}

#Drop the loaded database from mysql
sub delete_data {
	my( $source_dir, $location, $tablename, $dbpass ) = @_;
        
        #delete database
        run_command("$install_dir/bin/mysql -S $install_dir/var/lib/mysql/mysql.sock -u root -p$dbpass -e 'Drop database if exists LSST;'");
}

#load the partitioned pt11 data into qserv for use.  This does a number of
#steps all in one command.
sub load_data {
	my( $source_dir, $location, $tablename, $dbpass ) = @_;
	
	#check the stripes value
	my $stripes = get_value( 'stripes' );
	
	unless( $stripes ) {
		print "Error: sorry, the stripes value is unknown, please set it with the 'set' option.\n";
		return;
	}
	
	#create database if it doesn't exist
	run_command("$install_dir/bin/mysql -S $install_dir/var/lib/mysql/mysql.sock -u root -p$dbpass -e 'Create database if not exists LSST;'");
	
	#check on the table def, and add need columns
	print "Copy and changing $source_dir/${tablename}.sql\n";
	my $tmptable = lc $tablename;
	`cp $source_dir/${tablename}.sql $install_dir/tmp`;
	`perl -pi -e 's,^.*_chunkId.*\n,,' $install_dir/tmp/${tablename}.sql`;
	`perl -pi -e 's,^.*_subChunkId.*\n,,' $install_dir/tmp/${tablename}.sql`;
	`perl -pi -e 's,chunkId,dummy1,' $install_dir/tmp/${tablename}.sql`;
	`perl -pi -e 's,subChunkId,dummy2,' $install_dir/tmp/${tablename}.sql`;
	`perl -pi -e 's!^\(\\s*PRIMARY\)!  chunkId int\(11\) default NULL,\\n\\1!' $install_dir/tmp/${tablename}.sql`;
	`perl -pi -e 's!^\(\\s*PRIMARY\)!  subChunkId int\(11\) default NULL,\\n\\1!' $install_dir/tmp/${tablename}.sql`;

	if( $tablename eq 'Object' ) {
		`perl -pi -e 's,^.*PRIMARY.*\n,,' $install_dir/tmp/${tablename}.sql`;
		open TMPFILE, ">>$install_dir/tmp/${tablename}.sql";
		print TMPFILE "\nCREATE INDEX obj_objectid_idx on Object ( objectId );\n";
		close TMPFILE;
	}

	#regress through looking for partitioned data, create loading script
	open LOAD, ">$install_dir/tmp/${tablename}_load.sql";
	
	my %chunkslist = ();
	opendir DIR, "$location";
	my @dirslist = readdir DIR;
	closedir DIR;
	
	#look for paritioned table chunks, and create the load data sqls.
	foreach my $dir ( sort @dirslist ) {
		next if( $dir =~ /^\./ );
		
		if( $dir =~ /^stripe/ ) {
			opendir DIR, "$location/$dir";
			my @filelist = readdir DIR;
			closedir DIR;
			
			foreach my $file ( sort @filelist ) {
				if( $file =~ /(\w+)_(\d+).csv/ ) {
					my $loadname = "${1}_$2";
					if( $1 eq $tablename ) {
					print LOAD "DROP TABLE IF EXISTS $loadname;\n";
					print LOAD "CREATE TABLE IF NOT EXISTS $loadname LIKE $tablename;\n";
					print LOAD "LOAD DATA INFILE '$location/$dir/$file' IGNORE INTO TABLE $loadname FIELDS TERMINATED BY ',';\n";

					$chunkslist{$2} = 1;
					}
					if( $tablename eq 'Object' && $1 =~ /^Object\S+Overlap/ ) {
						print LOAD "CREATE TABLE IF NOT EXISTS $loadname LIKE $tablename;\n";
						print LOAD "LOAD DATA INFILE '$location/$dir/$file' INTO TABLE $loadname FIELDS TERMINATED BY ',';\n";
					}
				}
			}
		}
	}
	print LOAD "CREATE TABLE IF NOT EXISTS ${tablename}_1234567890 LIKE $tablename;\n";
	close LOAD;
	
	#load the data into the mysql instance
	print "Loading data, this make take awhile...\n";
	run_command("$install_dir/bin/mysql -S $install_dir/var/lib/mysql/mysql.sock -u root -p$dbpass LSST < $install_dir/tmp/${tablename}.sql");
	run_command("$install_dir/bin/mysql -S $install_dir/var/lib/mysql/mysql.sock -u root -p$dbpass LSST < $install_dir/tmp/${tablename}_load.sql");
		
	#create the empty chunks file
	unless( -e "$install_dir/etc/emptyChunks.txt" ) {
		create_emptychunks( $stripes, \%chunkslist );
	}
	
	#create a setup file
	unless( -e "$install_dir/etc/setup.cnf" ) {
		open SETUP, ">$install_dir/etc/setup.cnf";
		print SETUP "host:localhost\n";
		print SETUP "port:$mysql_proxy_port\n";
		print SETUP "user:root\n";
		print SETUP "pass:$dbpass\n";
		print SETUP "sock:$install_dir/var/lib/mysql/mysql.sock\n";
		close SETUP;
	}

	#check if database is already registered, if it is it needs to get unreg. first.
	#register the database, export
	run_command("$install_dir/bin/fixExportDir.sh");
		
}

#Create the empty chucks list up to 1000, and print this into the
#empty chunks file in etc.
sub create_emptychunks {
	my( $stripes, $chunkslist ) = @_;
	
	my $top_chunk = 2 * $stripes * $stripes;
		
	open CHUNKS, ">$install_dir/etc/emptyChunks.txt";
	for( my $i = 0; $i < $top_chunk; $i++ ) {
		unless( defined $chunkslist->{$i} ) {
			print CHUNKS "$i\n";
		}
	}
	close CHUNKS;
	
}

#routine to get the value from the config file
sub get_value {
	my( $text ) = @_;
	
	open CNF, "<$install_dir/etc/local.qserv.cnf";
	while( my $line = <CNF> ) {
		if( $line =~ /^$text\s*=\s*(\d+)/ ) {
			close CNF;
			return $1;
		}
	}
	close CNF;
	
}

#help report for the --help option
sub usage {
  my($exit, $message) = @_;

        my($bin) = ($0 =~ m!([^/]+)$!);
        print STDERR $message if defined $message;
        print STDERR <<INLINE_LITERAL_TEXT;     
usage: $bin [options]
  Help admin the qserv server install, starting, stopping, and checking of status
  Also supports the loading of pt11 example data into the server for use.

Options are:
      --debug         Print out debug messages.
  -h, --help          Print out this help message.
      --set           Set the stripes and substripes values, needs strips and substripes options.
  -s, --status        Print out the status of the server processes.
      --stop          Stop the servers.
	  --start         Start the servers.
	  --stripes       Number of stripes used to partition the data.
	  --substripes     Number of substripes used to partition the data.
	  --load          Load data into qserv, requires options source, stripedir, table
      --delete-data   Load data into qserv, requires options source, output, table
	  --source        Path to the pt11 exmple data
 --sdir, --stripedir  Path to the paritioned data
      --mysql-proxy-port  Port number to use for the mysql proxy.
	  --table         Table name for partitioning and loading
	  --parition      Partition the example pt11 data into chunks, requires source, stripedir, table
	  --test          test the use of the util, without performing the actions.

Examples: $bin --status

Comments to Douglas Smith <douglas\@slac.stanford.edu>.
INLINE_LITERAL_TEXT

	       exit($exit) if defined $exit;

}

sub run_command {
	my( $command ) = @_;
	my $cwd = cwd();
	my @return;
	print "-- Running: $command in $cwd\n";
	open( OUT, "$command |" ) || die "ERROR : can't fork $command : $!";
	while( <OUT> ) {
		print STDOUT $_; 
		push( @return, $_ ); 
	}
	close( OUT ) || die "ERROR : $command exits with error code ($?)";
	return @return;
}
