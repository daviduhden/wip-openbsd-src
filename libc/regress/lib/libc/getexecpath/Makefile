PROG=getexecpath

WARNINGS=yes

CLEANFILES+=wrap.sh

# Compare the physical executable path for direct execution and for
# shebangs using a relative path, redundant slashes, or env. wrap.in
# is a template for the kernel interpreter test. Generate each variant
# as an executable in the object directory.
run-regress-${PROG}:	${PROG}
	./${PROG} `cd ${.OBJDIR} && pwd -P`/${PROG}
	cp ${.CURDIR}/wrap.in ${.OBJDIR}/wrap.sh
	chmod +x ${.OBJDIR}/wrap.sh
	./wrap.sh `cd ${.OBJDIR} && pwd -P`/${PROG}
	sed 's!./${PROG}!///${.OBJDIR}/${PROG}!' ${.CURDIR}/wrap.in > ${.OBJDIR}/wrap.sh
	./wrap.sh `cd ${.OBJDIR} && pwd -P`/${PROG}
	sed 's!./${PROG}!/usr/bin/env ./${PROG}!' ${.CURDIR}/wrap.in > ${.OBJDIR}/wrap.sh
	./wrap.sh `cd ${.OBJDIR} && pwd -P`/${PROG}

.include <bsd.regress.mk>
