a.out: parsetest.o y.tab.o lex.yy.o errormsg.o util.o absyn.o symbol.o table.o prabsyn.o types.o env.o semant.o temp.o translate.o x86frame.o escape.o
	cc -g parsetest.o y.tab.o lex.yy.o errormsg.o util.o absyn.o symbol.o table.o prabsyn.o types.o env.o semant.o temp.o translate.o x86frame.o escape.o

parsetest.o: parsetest.c errormsg.h util.h
	cc -g -c parsetest.c

y.tab.o: y.tab.c
	cc -g -c y.tab.c

y.tab.c: tiger.grm
	yacc -dv tiger.grm

y.tab.h: y.tab.c
	echo "y.tab.h was created at the same time as y.tab.c"

errormsg.o: errormsg.c errormsg.h util.h
	cc -g -c errormsg.c

lex.yy.o: lex.yy.c y.tab.h errormsg.h util.h
	cc -g -c lex.yy.c

lex.yy.c: tiger.lex
	lex tiger.lex

util.o: util.c util.h
	cc -g -c util.c

symbol.o: symbol.c symbol.h
	cc -g -c symbol.c

absyn.o: absyn.c absyn.h symbol.h util.h
	cc -g -c absyn.c

prabsyn.o: prabsyn.c prabsyn.h util.h symbol.h absyn.h
	cc -g -c prabsyn.c

table.o: table.c table.h util.h
	cc -g -c table.c

types.o: types.c types.h util.h symbol.h
	cc -g -c types.c

env.o: env.c env.h
	cc -g -c env.c

semant.o: semant.c semant.h util.h symbol.h absyn.h types.h env.h
	cc -g -c semant.c

temp.o: temp.c temp.h util.h symbol.h table.h
	cc -g -c temp.c

translate.o: translate.c translate.h frame.h util.h symbol.h temp.h
	cc -g -c translate.c

x86frame.o: x86frame.c frame.h util.h symbol.h temp.h
	cc -g -c x86frame.c

escape.o: escape.c escape.h util.h symbol.h absyn.h
	cc -g -c escape.c

clean:
	rm -f a.out util.o parsetest.o lex.yy.o errormsg.o y.tab.c y.tab.h y.tab.o absyn.o symbol.o table.o prabsyn.o types.o env.o semant.o temp.o translate.o x86frame.o escape.o
