#include "structure/array.h"
#include "structure/string.h"
#include "misc/listopr.h"
#include "defs.h"
#include "io/fileStream.h"
#include "io/stringStream.h"
#include "io/memoryStream.h"
#include "io/terminal.h"
#include "misc/converter.h"
using namespace converter;
int main()
{
	void* membuffer = mem_malloc(655360);
	textStream fs;
	fs.open("/home/sleepwalking/dog", READONLY);
	fs.readChars((char*)membuffer);

	stringStream ss(membuffer);
	int i = 0;
	string wholestr = ss.readAll();
	string tmpstr;
	while(ss.getPosition() < fs.getLength())
	{
		tmpstr = ss.readWord();
		//wr(upperCase(tmpstr));
		//wr(" ");
		if(instr(tmpstr, CStr("stupid")) >= 0 || instr(tmpstr, CStr("Stupid")) >= 0 )
		{
			i ++;
			//terminal::readLine();
		}
	}
	wLine(CStr("There are ") + CStr(i) + " occurrences in The Curious Incident of the Dog in the Night-time.");

	int loc = instrRev(wholestr, CStr("because"));
	wLine(CStr("Last because: ") + CStr(loc) + CStr("\t") + mid(wholestr, loc - 50, 100));
	fs.close();
	mem_free(membuffer);

	string a = "  sfe ewfw wef   ";
	wLine(CStr("ltrim: '") + ltrim(a) + "'");
	wLine(CStr("rtrim: '") + rtrim(a) + "'");
	wLine(CStr("trim: '") + trim(a) + "'");

	array<string> passage;
	split(wholestr, passage);

	lfor(i, 50, wLine(passage[i]););
	
	return 0;
}
