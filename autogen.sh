source_dir="$(dirname $0)"
master_dir="`pwd`"
export GIT_DIR=${source_dir}/.git

if [ ! -e ${source_dir}/configure.ac ]; then
	echo "This script has to be placed in the top-level source directory"
	exit 1
fi

if [ -e Makefile ]; then
	echo "Running make distclean"
	make -s distclean
fi

echo "Running aclocal"
cd ${source_dir} && aclocal || exit 1
echo "Running autoheader"
autoheader || exit 1
echo "Running autoconf"
autoconf || exit 1
echo "Running automake --add-missing"
automake --add-missing --copy --foreign || exit 1
echo "Running ${source_dir}/configure $@"
cd ${master_dir} && ${source_dir}/configure "$@"
