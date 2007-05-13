#!/bin/sh

#
# this is a *very* simple re-implementation of mysql-test-run
#
# we need to have this as none of the mysql-test-run variants
# is installed by "make install" or part of our distribution
# packages
#
# currently we simply rely on all needed binaries to be in PATH
# although this is not necessarily the case or the ones found
# in PATH are not from the version the source was configured
# against
#

HERE=`pwd`
DATA=$HERE/var
SOCK=$DATA/sock

for a in mysql mysqld mysqladmin mysql_install_db mysqltest
do
  if ! which $a 
  then
    echo no '$a' binary in PATH
    exit 3
  fi
done

echo
echo "*** testing ***"
echo
export LD_LIBRARY_PATH=../../.libs/:$LD_LIBRARY_PATH

rm -rf $DATA
mkdir $DATA

mysql_install_db --datadir=$DATA > $DATA/test.err

mysqld --skip-networking --skip-innodb \
       --datadir=$DATA --socket=$SOCK \
       2>> $DATA/test.err &

sleep 5

mysql --socket=$SOCK -e "CREATE DATABASE IF NOT EXISTS test;"

for test in t/*.test
do
	name=`basename $test .test`
	echo -n "[$name] "
	result=r/$name.result
	mysqltest --socket=$SOCK --database test --test-file=$test --result-file=$result 
done
echo

mysqladmin -u root --socket=$SOCK shutdown
