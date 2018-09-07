#!/usr/bin/python

from lxml import etree
import requests

class GrokScraper:
    __slots__ = ['url', 'domain', 'proj', 'path', 'nresults', 'errors', 'getRevisionText']
    
    def __init__(self, **kwargs):
        #different grok versions might place the revision data in a different part of the DOM relative to the line elements, so use this as a means to customize the extraction code
        self.getRevisionText = lambda occurence: ocurrence.getnext().iterchildren().__next__()
        for name, val in kwargs.iteritems():
            if name in self.__slots__:
                setattr(self, name, val)
            else:
                raise RuntimeError('Invalid attribute <%s> for class GrokScraper!' % str(name))
        
    def getOcurrences(self, term):
        table = dict()
        if self.domain != '':
            domain = '/' + self.domain
        else:
            domain = ''
        url = "%s%s/search?q=%s&project=%s&defs=&path=%s&hist=&n=%d" % (self.url, domain, term, self.proj, self.path, self.nresults)
        r = requests.get(url)
        if r.ok:
            root = etree.HTML(r.text.encode('ascii', errors='replace'))
            xp = ".//tt/a/b[text()='%s']/.." % term
            occs = root.xpath(xp)
            for occ in occs:
                encoded = occ.attrib['href']
                split = encoded.split('#', 1)
                path = split[0]
                line = int(split[1])
                if path in table:
                    table[path].add(line)
                else:
                    table[path] = set([line])
        else:
            handleError(url, r, self.errors)
        return table


    def getRevisions(self, table):
        revisions = dict()
        for f, lines in table.iteritems():
            #This assumes that grok is responsive serving annotated sources for all affected ones. Thia might not be the case for some files, and these should be filtered out here
            url = "%s%s?a=true" %(self.url, f)
            r = requests.get(url)
            if r.ok:
                root = etree.HTML(r.text.encode('ascii', errors='replace'))
                for line in lines:
                    xp = ".//a[@class='l' and @name='%d']" % line
                    occs = root.xpath(xp)
                    if (len(occs)==0):
                        #this is cheaper than using a more complex boolean condition like ".//a[(@class='l' or @class='hl') and @name='%d']"
                        xp = ".//a[@class='hl' and @name='%d']" % line
                        occs = root.xpath(xp)
                    #I wouldn't need this contraption if I could express in xpath how to locate an element relative to an adjacent one, something like ".//a[@class='l' and @name='%d']/following::span/a"
                    occ = self.getRevisionText(occs[0])
                    rev = occ.text
                    comment = occ.attrib['title']
                    if not rev in revisions:
                        revisions[rev] = comment.replace('\x0a', ' ').replace('<br/>', '\n')
            else:
                handleError(url, r, self.errors)
        return revisions

def handleError(url, r, errors):
    if errors!='ignore':
        msg = "Error retrieving grok data for <%s>: %d %s" % (url, r.status_code, r.reason)
        if errors=='raise':
            raise RuntimeError()
        elif errors=='print':
            print msg

def printOcurrences(table):
    print "NUMBER OF FILES: %d\n" % len(table)
    for f in sorted(table.keys()):
        lines = list(table[f])
        lines.sort()
        print "occurences in file <%s> in lines:\n  %s" % (f, str(lines))

def printRevisions(revisions):
    print "NUMBER OF REVISIONS: %d\n" % len(revisions)
    i = 0
    for rev, comment in revisions.iteritems():
        print "---------------------------"
        print "REVISION %d: %s" % (i, rev)
        print comment
        print "\n"
        i=i+1
    print "---------------------------"
    
#########################################################

if __name__=='__main__':
    
    def BXR_SU():
        gs = GrokScraper(url='http://bxr.su', domain='', proj='FreeBSD', path='', nresults=100, errors='print')
        table = gs.getOcurrences('rt2561s')
        printOcurrences(table)
    
    def OPENGROK_LIBREOFFICE_ORG():
        gs = GrokScraper(url='https://opengrok.libreoffice.org', domain='', proj='cppunit', path='', nresults=100, errors='print')
        table = gs.getOcurrences('write')
        printOcurrences(table)
        revisions = gs.getRevisions(table)
        printRevisions(revisions)
    
    grokExamples = [BXR_SU, OPENGROK_LIBREOFFICE_ORG]
    i = 0
    for example in grokExamples:
        print "EXAMPLE NUMBER %d FOR GROK SCRAPER: %s" % (i, example.__name__)
        print "\n"
        example()
        i=i+1
