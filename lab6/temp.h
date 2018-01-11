/*
 * temp.h 
 *
 */

#ifndef TEMP_H
#define TEMP_H

#include "symbol.h"

typedef struct Temp_temp_ *Temp_temp;
typedef struct Temp_tempList_ *Temp_tempList;
typedef S_symbol Temp_label;
typedef struct Temp_labelList_ *Temp_labelList;
typedef struct Temp_map_ *Temp_map;


struct Temp_tempList_ { Temp_temp head; Temp_tempList tail;};
struct Temp_labelList_ { Temp_label head; Temp_labelList tail;};


Temp_tempList Temp_TempList(Temp_temp h, Temp_tempList t);
Temp_labelList Temp_LabelList(Temp_label h, Temp_labelList t);


Temp_temp Temp_newtemp(void);
Temp_label Temp_newlabel(void);
Temp_label Temp_newNamedLabel(string name);
Temp_label Temp_namedlabel(string name);
string Temp_labelstring(Temp_label s);

Temp_map Temp_empty(void);
Temp_map Temp_layerMap(Temp_map over, Temp_map under);
void Temp_enter(Temp_map m, Temp_temp t, string s);
string Temp_look(Temp_map m, Temp_temp t);
void Temp_dumpMap(FILE *out, Temp_map m);

Temp_map Temp_name(void);

#endif
