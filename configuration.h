#ifndef  _CONFIGURATION_H
#define  _CONFIGURATION_H

//A structure that stores file config variables from EEPROM
typedef struct{
	unsigned long newFileTime;      //Keeps track of when a newfile should be made.
	char workingFilename[19];       //The path and filename of the current week's file
} configuration;

#endif  //_CONFIGURATION_H
