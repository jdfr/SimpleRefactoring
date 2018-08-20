#!/usr/bin/python

from lxml import etree
import grokscrap as gs
import os
import subprocess as subp

class ExternalRefactor:
    def __init__(self, 
                 translatepath=lambda x: x, 
                 translatesavepath=lambda x: x, 
                 compiler_args_base=[], 
                 compiler_args=lambda x: ["-I."], 
                 command='./simpleRefactor', 
                 exedir=os.getcwd(), 
                 cppextensions=('.cpp',), 
                 isxmlfile=lambda x: x.endswith(('.xml',)), 
                 xml_xpath=None, 
                 simulate=False):
        self.translatepath = translatepath
        self.translatesavepath = translatesavepath
        self.compiler_args_base = compiler_args_base
        self.compiler_args = compiler_args
        self.command = command
        self.exedir = exedir
        self.cppextensions = cppextensions
        self.xml_xpath = xml_xpath
        self.isxmlfile = isxmlfile
        self.simulate = simulate
        self.template = lambda term, value, filepath: [self.command, '--term=%s' % term, '--value=%s' % value, filepath, '--']
    
    def doCPPFile(self, term, value, filepath, savepath):
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
    
    def doXMLFile(self, term, filepath, savepath):
        if self.simulate:
            print 'ON %s REMOVE REFERNCES TO %s' % (filepath, term)
        else:
            root = etree.parse(filepath)
            res = root.xpath(self.xml_xpath % term)
            if len(res)!=1:
                print "Error locating config value <%s> in XML file <%s>!!!!!" % (term, filepath)
            else:
                toremove = res[0]
                #prev = toremove.getprevious()
                #if prev is not None and prev.tail is not None:
                #    toks = prev.tail.rsplit('\n', 1)
                #    if len(toks)>1:
                #        prev.tail = toks[0]+'\n'
                toremove.getparent().remove(toremove)
                with open(savepath, 'w') as svp:
                    svp.write(etree.tostring(root))

    def doFilesFromTable(self, table, term, value):
        print "PROCESSING FILES FROM TERMS FOUND WITH OPENGROK\n"
        for grokfilepath in sorted(table.keys()):
            lines = list(table[grokfilepath])
            lines.sort()
            filepath = self.translatepath(grokfilepath)
            savepath = self.translatesavepath(grokfilepath)
            if grokfilepath.endswith(self.cppextensions):
                print "  TERM <%s> TO BE REFACTORED IN CPP FILE <%s> in line(s) %s" % (term, filepath, lines)
                self.doCPPFile(term, value, filepath, savepath)
            elif self.isxmlfile(filepath):
                print "  TERM <%s> TO BE REFACTORED IN XML FILE <%s> in line(s) %s" % (term, filepath, lines)
                self.doXMLFile(term, filepath, savepath)

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
    external = ExternalRefactor(translatepath=translatepath, translatesavepath=translatepath, compiler_args=lambda x: ["-I./examples/include"], xml_xpath="/config/value[@name='%s']", simulate=False)
    table = {'test.cpp':[], 'config.xml':[]}
    external.doFilesFromTable(table, "UseSpanishLanguage", "true") 
