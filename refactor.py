#!/usr/bin/python

from lxml import etree
import grokscrap as gs
import os
import subprocess as subp
import re

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

    def checkStep(self):
        if (self.execute):
            if self.resumeFrom==self.step:
                self.resumeFrom=None
        return self.execute and self.resumeFrom is None

    def updateStep(self):
        self.writeStep(self.step+1)

    def actualDoCommand(self, command, **kwargs):
        #this is intended to actually do subprocess call, as some esoteric special-needs tools might be so picky about how exactly they are invoked that you can't yjust assume that a straight subptocess.Popen/call will work
        #default implementation just uses regular subprocess.call
        return subp.call(command, **kwargs)

    def doCommand(self, command, **kwargs):
        if self.verbose:
            print ' '.join(command)
        if self.checkStep():
            ret = self.actualDoCommand(command, **kwargs)
            if ret!=0:
                raise RuntimeError("Error in command <%s>" % ' '.join(command))
        self.updateStep()

    def doCd(self, dr):
        if self.checkStep() or True: #as chdirs have critically important side-effects, they have to be replayed no matter what
            os.chdir(dr)
        self.updateStep()
        if self.verbose:
            print "cd "+dr

#simple but high-level driver for the refactor binary, it basically does housekeeping and high-level planning; the binary does the grunt work.
class ExternalRefactor:
    def __init__(self, 
                 context,
                 translatepath=lambda x: x, 
                 compiler_args_base=[], 
                 compiler_args=lambda x: ["-I."], 
                 command='./simpleRefactor', 
                 exedir=os.getcwd(), 
                 cppextensions=('.cpp',), 
                 hppextensions=('.hpp', '.h'),
                 getCppForHpp=None,
                 grokscraper=None,
                 isxmlfile=lambda x: x.endswith(('.xml',)), 
                 xml_xpath=None, 
                 execute=True,
                 verbose=True):
        self.context = context
        self.translatepath = translatepath
        self.compiler_args_base = compiler_args_base
        self.compiler_args = compiler_args
        self.command = command
        self.exedir = exedir
        self.cppextensions = cppextensions
        self.hppextensions = hppextensions
        self.getCppForHpp = getCppForHpp
        self.grokscraper = grokscraper
        self.xml_xpath = xml_xpath
        self.isxmlfile = isxmlfile
        self.execute = execute
        self.verbose = verbose
        self.template = lambda term, value, filepath: [self.command, '--term=%s' % term, '--value=%s' % value, '--overwrite=true', filepath, '--']

    #do not forget to call this one if you want to make sure to also refactor instances that only apper in header files!
    def addCppFilesForHppFiles(self, table):
        for filename in table:
            if filename.endswith(self.hppextensions):
                cpp = None
                if self.getCppForHpp is not None:
                    cpp = self.getCppForHpp(filename) #paths returned here should be consistent with grok, rather than with the codebase
                elif self.grokscraper is not None:
                    cpp = self.getCppForHppWithGrok(filename, table)
                if cpp is None:
                    raise RuntimeError("Could not find a C++ source file including the header %s!!!!!" % filename)
                if cpp in table:
                    table[cpp].add(filename)
                else:
                    table[cpp] = set([filename])

    def getCppForHppWithGrok(self, hppfile, table):
        filename = hppfile.rsplit('/', 1)[1]
        hpptable = self.grokscraper.getOcurrences("include "+filename)
        #first check if the file is already in the table
        for grokfilepath in hpptable:
            if grokfilepath.endswith(self.cppextensions) and grokfilepath in table:
                return grokfilepath
        #this is a quite dumb brute-force approach, it might be far better to avoid greedy strategies and compute a minimal set of cpps for all hpps with ocurrences; however that might be inefficient for codebases with massively nested sets of header files
        for grokfilepath in hpptable:
            if grokfilepath.endswith(self.cppextensions):
                return grokfilepath
        for grokfilepath in hpptable:
            if grokfilepath.endswith(self.hppextensions):
                ret = self.getCppForHppWithGrok(grokfilepath)
                if ret is not None:
                    return ret
        #there might be headers not included anywhere in the codebase (conceivably, they might be included by source files generated during the build process). If that's the case, here we should add some code to (a) use those generated sources (after compilation) or (b) generate some phony C++ source file that just includes the header and feed it to the binary tool 
        return None

    def doCPPFile(self, term, value, filepath):
        commandline = self.template(term, value, filepath)+self.compiler_args_base+self.compiler_args(filepath)
        if self.verbose:
            print 'ON %s EXECUTE %s' % (self.exedir, ' '.join(commandline))
        if self.execute:
            self.context.doCommand(commandline, cwd=self.exedir)

    def doXMLFile(self, term, filepath):
        if self.verbose:
            print 'ON %s REMOVE REFERNCES TO %s' % (filepath, term)
        if self.execute:
            if self.context.checkStep():
                root = etree.parse(filepath)
                res = root.xpath(self.xml_xpath % term)
                if len(res)!=1:
                    print "Error locating config value <%s> in XML file <%s>!!!!!" % (term, filepath)
                else:
                    toremove = res[0]
                    toremove.getparent().remove(toremove)
                    with open(filepath, 'w') as svp:
                        svp.write(etree.tostring(root))
            self.context.updateStep()

    #main function, does the refactoring
    def doFilesFromTable(self, table, term, value):
        if self.verbose:
            print "PROCESSING FILES FROM TERMS FOUND WITH OPENGROK\n"
        for grokfilepath in sorted(table.keys()):
            lines = list(table[grokfilepath])
            lines.sort()
            filepath = self.translatepath(grokfilepath)
            if grokfilepath.endswith(self.cppextensions):
                if self.verbose:
                    print "  TERM <%s> TO BE REFACTORED IN CPP FILE <%s> in line(s) %s" % (term, filepath, lines)
                self.doCPPFile(term, value, filepath)
            if grokfilepath.endswith(self.hppextensions):
                if self.verbose:
                    print "  TERM <%s> FOUND IN HEADER FILE <%s> in line(s) %s (refactored as part of a cpp file)" % (term, filepath, lines)
            elif self.isxmlfile(filepath):
                if self.verbose:
                    print "  TERM <%s> TO BE REFACTORED IN XML FILE <%s> in line(s) %s" % (term, filepath, lines)
                self.doXMLFile(term, filepath)

    #an example of how a high-level funtion to use GrokScraper and ExternalRefactor might look like
    def doFilesFromGrok(self, term, value, printRevs=True):
        table = grokscraper.getOcurrences(term)
        self.addCppFilesForHppFiles(table)
        self.context.startStep()
        self.doFilesFromTable(table, term, value)
        self.context.endStep()
        if printRevs:
            print ""
            revisions = self.grokscraper.getRevisions(table)
            self.grokscraper.printRevisions(revisions)


#helper funtion to be used as part of function compiler_args (one of the members of ExternalRefactor): for a .d file (the one generated by the compiler detailing ALL files #included into the compilation unit), heuristically generate a list of directives to include the relevant directories
def heuristicIncludeDirListFromDFile(dfilepath, rootDirKeyword=['include/'], hppextensions=(('.hpp', '.h')), toExclude=[], prefix=lambda x: ''):
    with open(dfilepath, 'r') as fd:
        text = fd.read()
    dirs_unfiltered = set()
    dirs_filtered = set()
    for m in re.finditer('[_\./a-zA-Z0-9-]+', text):
        path = m.group(0)
        if path.endswith(hppextensions):
            dr = path.rsplit('/', 1)[0]
            if not dr.endswith(hppextensions) and not dr in dirs_unfiltered:
                dirs_unfiltered.add(dr)
                toAdd = True
                for excl in toExclude:
                    if dr.startswith(excl):
                        toAdd = False
                        break
                if toAdd:
                    processed = False
                    for key in rootDirKeyword:
                        pos = dr.find(key)
                        if pos!=-1:
                            processed = True
                            dr = dr[:pos+len(key)]
                            break
                    if dr[-1]=='/':
                        dr = dr[:-1]
                    dirs_filtered.add(dr)
    return ['-I'+prefix(dr)+dr for dr in dirs_filtered]


#########################################################

if __name__=='__main__':
    
    translatepath = lambda x: os.path.join('examples', x)
    context = ExecuteContext(verbose=True)
    external = ExternalRefactor(context,
                                translatepath=translatepath,
                                compiler_args=lambda x: ["-I./examples/include"], #if we used a non-installed binary release of clang (i.e. a clang release we just unzipped somewhere where we happen to have write permissions) to compile the binary tool, we should put in this list an additionsl include pointing to the location where clang's stddef.h lives inside the clang directory tree
                                xml_xpath="/config/value[@name='%s']",
                                execute=True,
                                verbose=True)
    table = {'test.cpp':[], 'config.xml':[]}
    external.doFilesFromTable(table, "UseSpanishLanguage", "true") 
