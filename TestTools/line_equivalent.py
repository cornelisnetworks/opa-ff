import sys
import os
import tempfile
dict = {0:0}

# Help Text
def usage():
  print "Usage: python line_equivalent.py <file_name> <line_number> <buildFeatureDefs>\n"
  print "The tool is used to find the equivalent line number for stripped file"
  print "Input: file name, line number of stripped file (customer copy) and buildFeatureDefs file"
  print "Output: line number in the code used by developers"
  print "Note: make sure the buildFeatureDefs is the same that was used to create the customer release"

# Create a hash table line_num_of_stripped_file:line_num_of_orig_file
def create_table(file_name, buildFeatureDefs):
  # Create a temporary file, to store unifdef output
  temp_file = tempfile.NamedTemporaryFile()
  # Use unifdef to strip the file based on buildFeatureDefs,
  # This temporary file should look same as the one in RPM 
  cmd="unifdef -f %s %s > %s"%(buildFeatureDefs, file_name, temp_file.name)
  os.system(cmd)
  # Create table
  line_num_temp_file=0
  line_num_orig_file=0
  with open(file_name) as orig_file, temp_file:
    for orig_line in iter(orig_file.readline, ''):
      line_num_orig_file += 1
      last_pos = temp_file.tell()
      if orig_line != temp_file.readline():
        temp_file.seek(last_pos)
        continue
      line_num_temp_file += 1
      dict[line_num_temp_file] = line_num_orig_file

# Query hash_table
def query_table(line_number):
  if int(line_number) in dict:
    print dict[int(line_number)]
  else:
    print "Invalid query: Line number do not exist in the stripped file"

# main
if len(sys.argv) != 4:
  usage()
  exit()

# Collect Input
file_name = sys.argv[1]
line_number = sys.argv[2]
buildFeatureDefs=sys.argv[3]

# Check if file exists
if not os.path.isfile(file_name):
  print "Invalid file path - %s\nprovide complete path to the file"%file_name
  exit(1)
if not os.path.isfile(buildFeatureDefs):
  print "Invalid file path - %s\nprovide complete path to the file"%buildFeatureDefs
  exit(1)

create_table(file_name, buildFeatureDefs)
query_table(line_number)
