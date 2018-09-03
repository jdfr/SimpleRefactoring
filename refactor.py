#!/usr/bin/python

from lxml import etree
import grokscrap as gs
import os
import subprocess as subp

#This class allows for (very handy) re-entrant lists of command-line calls. All you need is to call startStep() at the beginning and make sure to call endStep() at the end only if there was no problem and the list doesn't have to be replayed. And, of course, do not change the list across runs, at least in the parts already executed, or hard-to-debug problems will ensue
class ExecuteContext:
    def __init__(self, execute=True, verbose=False, stampName=os.path.join(os.getcwd(), 'step.txt')):
        self.execute = execute
        self.verbose = verbose
        self.resumeFrom = None
        self.stampName = stampName
        self.step = 0

    def writeStep(self, n):
        self.step = n
        with open(self.stampName, 'w') as f:
            f.write(str(n))

    def startStep(self):
        if os.path.isfile(self.stampName):
            with open(self.stampName, 'r') as f:
                self.resumeFrom = int(f.read())
        else:
            self.resumeFrom = None
        self.writeStep(0)

    def endStep(self):
        if os.path.isfile(self.stampName):
            os.remove(self.stampName)

    def prepareStep(self):
        self.writeStep(self.step+1)
        if (self.execute):
                if self.resumeFrom==self.step:
                    self.resumeFrom=None
        return self.execute and self.resumeFrom is None

    def doCommand(self, command, **kwargs):
        if self.verbose:
            print ' '.join(command)
        if self.prepareStep():
            ret = subp.call(command, **kwargs)
            if ret!=0:
                raise RuntimeError("Error in command <%s>" % ' '.join(command))

    def doOverwritingCommand(slef, command, fileout, **kwargs):
        if self.verbose:
            print ' '.join(command)+' > '+fileout
        if self.prepareStep():
            output = subp.check_output(commandline, **kwargs)
            with open(fileout, 'w') as svp:
                svp.write(output)

    def doCd(self, dr):
        if (self.verbose):
            print "cd "+dr
        if self.prepareStep():
            os.chdir(dr)

#simple but high-level driver for the refactor binary, it basically does housekeeping and high-level planning; the binary does the grunt work.
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
