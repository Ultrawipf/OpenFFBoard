import csv
import argparse

# Top section of src file
CODEHEADER = """/*		
 * eeprom_addresses.c		
 *		
 *  Created on: 24.01.2020		
 *  Author: Yannick		
 *		
 *	/!\ Generated from the file memory_map.csv 
   / ! \ DO NOT EDIT THIS DIRECTLY !!!	
 */		
		
#include "eeprom_addresses.h"		

"""
# Top section of header file
HEADERHEADER = """/*		
 * eeprom_addresses.h		
 *		
 *  Created on: 24.01.2020		
 *  Author: Yannick		
 *		
 *	/!\ Generated from the file memory_map.csv 
   / ! \ DO NOT EDIT THIS DIRECTLY !!!	
 */			
		
#ifndef EEPROM_ADDRESSES_H_
#define EEPROM_ADDRESSES_H_

#include "main.h"
// Change this to the amount of currently registered variables
#define NB_OF_VAR {0}
extern const uint16_t VirtAddVarTab[NB_OF_VAR];

// Amount of variables in exportable list
#define NB_EXPORTABLE_ADR {1}
extern const uint16_t exportableFlashAddresses[NB_EXPORTABLE_ADR];


		
/* Add your addresses here. 0xffff is invalid as it marks an erased field.		
Anything below 0x00ff is reserved for system variables.		
		
Use ranges that are clear to distinguish between configurations. Address ranges can have gaps.		
Label the names clearly.		
Example: 0x0100 - 0x01ff for one class and 0x0200-0x02ff for another class would be reasonable even if they each need only 3 variables		
		
		
Important: Add your variable to the VirtAddVarTab[NB_OF_VAR] array in eeprom_addresses.c!		
		
Tip to check if a cell is intialized:		
uint16_t EE_ReadVariable(uint16_t VirtAddress, uint16_t* Data) will return 1 if the address is not found or 0 if it was found.		
*/
"""

# Generates the source file
def generatesrc(entries):
    result = CODEHEADER
    allvars = ""
    exportvars = ""

    allvarheader = """/*		
Add all used addresses to the VirtAddVarTab[] array. This is important for the eeprom emulation to correctly transfer between pages.		
This ensures that addresses that were once used are not copied again in a page transfer if they are not in this array.		
*/		
		
const uint16_t VirtAddVarTab[NB_OF_VAR] =		
{
"""

    exportableheader = """/**		
 * Variables to be included in a flash dump		
 */		
const uint16_t exportableFlashAddresses[NB_EXPORTABLE_ADR] =		
{
"""

    for entry in entries:
        line = ""
        if entry["sec"]: # Section comment
            line = entry["sec"] + "\n"
            allvars += line
            exportvars += line

        if entry["adr"] and entry["name"]: # Address line
            line = "\t"+entry["name"] + ","
            if(entry["comment"]): # Add comment
                if(entry["comment"][0:2] != "//"):
                    line += "//"
                line += " "+entry["comment"]
            line += "\n"

            if not entry["enable"] == "1": # Add to all list
                allvars += "//"
            allvars += line
            if not entry["export"] == "1": # Add to export list
                exportvars += "//"
            exportvars += line
    result += allvarheader + allvars + "};\n\n" + exportableheader + exportvars + "};"
    return result

# Generates the header file
def generate_header(entries):
    exportable = list(filter(lambda d: d['export'] == "1", entries))
    enabled = list(filter(lambda d: d['enable'] == "1", entries))
    nbvar = len(enabled)
    nbvarexport = len(exportable)
    result = HEADERHEADER.format(nbvar,nbvarexport)
    for entry in entries:
        if entry["sec"]: # Section comment
            result += entry["sec"] + "\n"
        if entry["adr"] and entry["name"]:
            line = f"#define {entry['name']} {entry['adr']}"
            if(entry["comment"]): # Add comment
                if(entry["comment"][0:2] != "//"):
                    line += "//"
                line += " "+entry["comment"]
            line += "\n"
            result += line
    result += "#endif /* EEPROM_ADDRESSES_H_ */\n"
    return result

# Parses the csv file for the other functions
def parse_csv(csvfile):
    result = []
    with open(csvfile,"r") as f:
        for line,row in enumerate(csv.reader(f,delimiter=",")):
            if(line == 0):
                continue # Skip header
            sec,name,adr,comment,enable,export = [r if r else None for r in row]
            result.append({"sec":sec,"name":name,"adr":adr,"comment":comment,"enable":enable,"export":export})
    return result

# Counts the number of duplicate addresses found. Should be zero
def check_duplicated(entries):
    addresses = [d["adr"] for d in entries if d["adr"] and (d["enable"] or d["export"])]
    duplicates = set([number for number in addresses if addresses.count(number) > 1])
    numberdupes = len(duplicates)
    if(numberdupes):
        print(f"Found {numberdupes} duplicate addresses: {duplicates}")
    return numberdupes


def main():
    parser = argparse.ArgumentParser(
                    prog='Memory map generator',
                    description='Generates src and header files for eeprom emulation')
    parser.add_argument('-f',"--filename",default="memory_map.csv")
    parser.add_argument('--srcfile',type = str,default = "eeprom_addresses.c")
    parser.add_argument('--hfile',type = str, default = "eeprom_addresses.h")
    args = parser.parse_args()

    entries = parse_csv(args.filename)

    if(check_duplicated(entries)):
        print("Aborting")
        return
    
    # Write src file
    with open(args.srcfile,"w") as f:
        f.write(generatesrc(entries))

    # Write header file
    with open(args.hfile,"w") as f:
        f.write(generate_header(entries))

    

if __name__ == "__main__":
    main()