#!/bin/sh

source_dir="$(dirname $0)"

if [ ! -e ${source_dir}/configure.ac ]; then
	echo "This script has to be placed in the top-level source directory"
	exit 1
fi

if [ -e Makefile ]; then
	echo "Running make distclean"
	make -s distclean
fi

pushd ${source_dir} >/dev/null
echo "Running aclocal"
aclocal || exit 1
echo "Running autoconf"
autoconf || exit 1
echo "Running autoheader"
autoheader || exit 1
echo "Running automake --add-missing --copy --foreign"
automake --add-missing --copy --foreign || exit 1
echo "Running ${source_dir}/configure $@"
popd >/dev/null
[ -n "${NOCONFIGURE}" ] && ${source_dir}/configure "$@"
