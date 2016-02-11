#!/usr/bin/python2

import os
import argparse
import re
import sys

# Input filtering
initialComment = True
def isInitialComment(line):
	global initialComment
	initialComment = initialComment and (line.startswith("/***") or line.startswith(" **") or line.startswith("**"))
	return initialComment;

forwardDeclarationRegex = re.compile('^class \w+;$')
def isForwardDeclaration(line):
	global forwardDeclarationRegex
	return forwardDeclarationRegex.match(line);
	
def isInclude(line):
	return line.startswith("#include ");

includesToPreserve = []

def emptyLine(line):
	emptyLineRegex = re.compile('^\s*$')
	return emptyLineRegex.match(line)

def isExcluded(line):
	global correspondingSource
	return isInitialComment(line) or emptyLine(line) or (len(includesToPreserve) > 0 and isInclude(line) and not (line in includesToPreserve)) # or isForwardDeclaration(line)

# Argument parsers
argParser = argparse.ArgumentParser('Sort the declarations in a source file')
argParser.add_argument('inputFile')
argParser.add_argument('outputFile')
argParser.add_argument('correspondingOriginalSource', nargs='?')

args = argParser.parse_args()
sourceText = ''

# If an original source was provided record all of its includes.
# These will be preserved in the currently processed file
if (args.correspondingOriginalSource):
	with open(args.correspondingOriginalSource, 'r') as correspondingSource:
		for line in correspondingSource:
			if isInclude(line):
				includesToPreserve.append(line)

# Read entire file in a single string
with open(args.inputFile, 'r') as inputFile:
	for line in inputFile:
		if not isExcluded(line):
			if len(sourceText) > 0 and not sourceText.endswith('\n'):
				sourceText += '\n'
			sourceText += line


# Block class used to store and various fragments of the file
class Block:
	methodRegex = re.compile(r'.*\)(\s|const|override)*\s*(=\s*\w+)?\s*{\n$', re.DOTALL)
	
	def __init__(self):
		self.prefix = ""
		self.suffix = ""
		self.children = []
		self.finalized = False
		self.isMethodHeader = False
	
	def add(self, char):
		if self.finalized:
			self.suffix += char
		else:
			self.prefix += char
			
	def removeLast(self, char):
		if self.finalized:
			assert self.suffix.endswith(char)
			self.prefix = self.prefix[:-1]
		else:
			assert self.prefix.endswith(char)
			self.prefix = self.prefix[:-1]
	
	def deepen(self):
		assert not self.finalized
		self.children.append(Block())
		return self.children[-1]
	
	def finalize(self):
		assert not self.finalized
		self.finalized = True
		self.isMethodHeader = (self.methodRegex.match(self.prefix) != None) # cast to boolean
		
	def debugPrint(self, indentation = ''):
		if not self.prefix and not self.suffix and not self.children:
			return
		sys.stdout.write(indentation + '>>>')
		sys.stdout.write(indentation + self.prefix)
		if self.isMethodHeader:
			sys.stdout.write(indentation + 'METHOD BODY')
		else:
			for c in self.children:
				assert c != self
				c.debugPrint(indentation + '\t')
			
		sys.stdout.write(indentation + self.suffix)
		sys.stdout.write(indentation + '<<<')
		
	def sort(self):
		if not self.isMethodHeader:
			self.children.sort(key = lambda x : x.sortingString())
			for c in self.children:
				c.sort()
			
	def sortingString(self):
		return self.prefix
			
	def text(self):
		return self.prefix + (''.join(c.text() for c in self.children))  + self.suffix
	
	def addEmptyLines(self):
		self.suffix += '\n'
		if not self.isMethodHeader:
			for c in self.children:
				c.addEmptyLines()

# Store current nesting level in a stack
top = Block() # Here we will collect all blocks
stack = [top, top.deepen()]

inString = False
escaped = False
quote = "" #Either " or '

inComment = False
lineComment = False

prev = '' # previous character
current = '' # current character

currentLine = ''

closingBraceRegex = re.compile(r'\s*\};?$', re.DOTALL)
doxyComment = re.compile(r'\s*/\*\*[^<].*\*/\s*$', re.DOTALL)

# Loop over all the characters and build the tree of Block structures
for char in sourceText:
	if len(currentLine) > 0  and currentLine[-1] == '\n':
		currentLine = ''
	
	prev = current
	current = char
	stack[-1].add(char)
	currentLine += current
	
	if inString:
		assert char != '\n'
		if escaped:
			escaped = False
			continue
		if char == quote:
			inString = False
			continue
		if char == '\\':
			escaped = True
			continue
		continue
	
	if inComment:
		if lineComment and char == '\n':
			lineComment = False
			inComment = False
			continue
		if not lineComment and prev == '*' and char == '/':
			inComment = False
			continue
		continue
	
	assert not inString and not escaped and not inComment and not lineComment
	
	if char == '/' and prev == '/':
		inComment = True
		lineComment = True
		continue
	
	if char == '*' and prev == '/':
		inComment = True
		continue
	
	if char == '"' or char =='\'':
		inString = True
		quote = char
		continue
	
	if char != '\n':
		continue
	
	# If this is just the end of a doxy comment, do not make a separate block for it
	if doxyComment.match(stack[-1].prefix):
		continue;
	
	# This is the end of the line and we are not in a comment/string and the last character is {
	# Start a new block
	if prev == '{':
		stack.append( stack[-1].deepen() )
		continue
	
	# Finish a block at every end of line.
	if closingBraceRegex.match(currentLine):
		stack[-1].removeLast(char) # The last } does not belong to the child block ...
		stack[-2].finalize()
		stack[-2].add(char) # ... it belongs to the parent block
		stack = stack[:-1] # pop the stack (child block)
	else:
		stack[-1].finalize()

	stack = stack[:-1] # pop the stack (peer block)
	stack.append( stack[-1].deepen()) # add a new peer
	continue

#top.debugPrint() # Use this to debug things

top.sort()
top.addEmptyLines()
with open(args.outputFile, 'w') as outputFile:
	outputFile.write(top.text())