#!/usr/bin/python

import grokscrap as gs
import os
import subprocess as subp

class ExternalRefactor:
    def __init__(self, translatepath=lambda x: x, translatesavepath=lambda x: x, compiler_args_base=[], compiler_args=lambda x: ["-I."], command='./simpleRefactor', exedir=os.getcwd(), extensions=('.cpp',), simulate=False):
        self.translatepath = translatepath
        self.translatesavepath = translatesavepath
        self.compiler_args_base = compiler_args_base
        self.compiler_args = compiler_args
        self.command = command
        self.exedir = exedir
        self.extensions = extensions
        self.simulate = simulate
        self.template = lambda term, value, filepath: [self.command, '--term=%s' % term, '--value=%s' % value, filepath, '--']
    
    def doFile(self, term, value, filepath, savepath):
        commandline = self.template(term, value, filepath)+self.compiler_args_base+self.compiler_args(filepath)
        if self.simulate:
            print 'ON %s EXECUTE %s' % (self.exedir, ' '.join(commandline))
        else:
            if os.path.samefile(filepath, savepath):
                try:
                    refactored = subp.check_output(commandline, cwd=self.exedir)
                    with open(savepath, 'w') as svp:
                        svp.write(refactored)
                except CalledProcessError as err:
                    print err.output
                    print "REFACTORING TOOL RETURNED ERROR CODE %d" % err.returncode
            else:
                subp.call(commandline, cwd=self.exedir, stdout=file(savepath, 'w'))

    def doFilesFromTable(self, table, term, value):
        print "PROCESSING FILES FROM TERMS FOUND WITH OPENGROK\n"
        for grokfilepath in sorted(table.keys()):
            lines = list(table[grokfilepath])
            lines.sort()
            filepath = self.translatepath(grokfilepath)
            savepath = self.translatesavepath(grokfilepath)
            if grokfilepath.endswith(self.extensions):
                print "  TERM <%s> TO BE REFACTORED IN FILE <%s> in line(s) %s" % (term, filepath, lines)
                self.doFile(term, value, filepath, savepath)
            else:
                print "  TERM <%s> APPEARS IN FILE <%s> in line(s) %s" % (term, filepath, lines)

    def doFilesFromGrok(self, grokscraper, term, value, printRevs=True):
        table = grokscraper.getOcurrences(term)
        doFilesFromTable(table, term, value)
        if printRevs:
            print ""
            revisions = grokscraper.getRevisions(table)
            gs.printRevisions(revisions)

#########################################################

if __name__=='__main__':
    
    translatepath = lambda x: os.path.join('examples', x)
    external = ExternalRefactor(translatepath=translatepath, translatesavepath=translatepath, compiler_args=lambda x: ["-I./examples/include"], simulate=False)
    table = {'test.cpp':[]}
    external.doFilesFromTable(table, "UseSpanishLanguage", "true") 
