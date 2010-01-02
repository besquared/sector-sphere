#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>

using namespace std;

int main()
{
   system("nohup ./start_master > /dev/null &");
   cout << "start master ...\n";

   ifstream ifs("../conf/slaves.list");

   if (ifs.bad() || ifs.fail())
   {
      cout << "no slave list found!\n";
      return -1;
   }

   int count = 0;

   while (!ifs.eof())
   {
      if (++ count == 64)
      {
         // wait a while to avoid too many incoming slaves crashing the master
         sleep(1);
         count = 0;
      }

      char line[256];
      line[0] = '\0';
      ifs.getline(line, 256);
      if (*line == '\0')
         continue;

      int i = 0;
      int n = strlen(line);
      for (; i < n; ++ i)
      {
         if ((line[i] != ' ') && (line[i] != '\t'))
            break;
      }

      if ((i == n) && (line[i] == '#'))
         continue;

      char newline[256];
      bool blank = false;
      char* p = newline;
      for (; i <= n; ++ i)
      {
         if ((line[i] == ' ') || (line[i] == '\t'))
         {
            if (!blank)
               *p++ = ' ';
            blank = true;
         }
         else
         {
            *p++ = line[i];
            blank = false;
         }
      }

      string base = newline;
      base = base.substr(base.find(' ') + 1, base.length());

      string addr = newline;
      addr = addr.substr(0, addr.find(' '));

      system((string("ssh ") + addr + " \"" + base + "/start_slave " + base + " &> /dev/null &\" &").c_str());

      cout << "start slave at " << addr << endl;
   }

   return 0;
}
