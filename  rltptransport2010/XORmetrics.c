#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Declaration of functions */
char* XORmetrics (char* hash1, char* hash2);


/* Main */
int main(int argc, char **argv){

char* hash1 = "AC1059B1";
char* hash2 = "01E15485";

char* result = XORmetrics(hash1,hash2);

	printf("%s\n", result);
	getch();
}


char* XORmetrics (char* hash1, char* hash2)
{
 	/* transformation of hash1 and hash2 (hexadecimal) into bits */
 	/* Declaration of variables */
 	int i1 = strlen(hash1);
 	int i2 = strlen(hash2);
 	int i;
	int temp1[i1*4];
 	int temp2[i2*4];
 	
 	int imax;
 	if (i1 < i2)
 	{
	   	   imax = i2;
  		   }
  		   else imax = i1;
 	
 	int BITdistance[imax*4];
	
 	char* HEXdistance = (char*)malloc(sizeof(char)*imax);
	
	/* Conversion of hash1 into bits */
		for(i=0; i < i1; i++)
	{
	 	int n0;
		int n1;  
		int n2;  
		int n3;
		
		switch (hash1[i])
	  {
	   		case '0' :  n3 = 0; n2 = 0; n1 = 0; n0 = 0;
 	 		 	 break;
   			case '1' :  n3 = 0; n2 = 0; n1 = 0; n0 = 1;
      			 break;
   			case '2' :  n3 = 0; n2 = 0; n1 = 1; n0 = 0;
      			 break;
   			case '3' :  n3 = 0; n2 = 0; n1 = 1; n0 = 1;
                 break;
   			case '4' :  n3 = 0; n2 = 1; n1 = 0; n0 = 0;
                 break;
   			case '5' :  n3 = 0; n2 = 1; n1 = 0; n0 = 1;
                 break;
   			case '6' :  n3 = 0; n2 = 1; n1 = 1; n0 = 0;
                 break;
   			case '7' :  n3 = 0; n2 = 1; n1 = 1; n0 = 1;
                 break;
   			case '8' :  n3 = 1; n2 = 0; n1 = 0; n0 = 0;
                 break;
   			case '9' :  n3 = 1; n2 = 0; n1 = 0; n0 = 1;
                 break;
			case 'A' :  n3 = 1; n2 = 0; n1 = 1; n0 = 0;
                 break;
   			case 'B' :  n3 = 1; n2 = 0; n1 = 1; n0 = 1;
                 break;
   			case 'C' :  n3 = 1; n2 = 1; n1 = 0; n0 = 0;
                 break;
   			case 'D' :  n3 = 1; n2 = 1; n1 = 0; n0 = 1;
                 break;
   			case 'E' :  n3 = 1; n2 = 1; n1 = 1; n0 = 0;
                 break;
   			case 'F' :  n3 = 1; n2 = 1; n1 = 1; n0 = 1;
                 break;
        default : printf("\nError in HBconversion\n");
		}

	 	temp1[4*i] = n0;
	 	temp1[4*i+1] = n1;
	 	temp1[4*i+2] = n2;
	 	temp1[4*i+3] = n3;
	 }
 		
	/* Conversion of hash2 into bits */	 
 		 for(i=0; i < i2; i++)
	{
	 	int n0;
		int n1;  
		int n2;  
		int n3;
		
		switch (hash2[i])
	  {
	   		case '0' :  n3 = 0; n2 = 0; n1 = 0; n0 = 0;
 	 		 	 break;
   			case '1' :  n3 = 0; n2 = 0; n1 = 0; n0 = 1;
      			 break;
   			case '2' :  n3 = 0; n2 = 0; n1 = 1; n0 = 0;
      			 break;
   			case '3' :  n3 = 0; n2 = 0; n1 = 1; n0 = 1;
                 break;
   			case '4' :  n3 = 0; n2 = 1; n1 = 0; n0 = 0;
                 break;
   			case '5' :  n3 = 0; n2 = 1; n1 = 0; n0 = 1;
                 break;
   			case '6' :  n3 = 0; n2 = 1; n1 = 1; n0 = 0;
                 break;
   			case '7' :  n3 = 0; n2 = 1; n1 = 1; n0 = 1;
                 break;
   			case '8' :  n3 = 1; n2 = 0; n1 = 0; n0 = 0;
                 break;
   			case '9' :  n3 = 1; n2 = 0; n1 = 0; n0 = 1;
                 break;
			case 'A' :  n3 = 1; n2 = 0; n1 = 1; n0 = 0;
                 break;
   			case 'B' :  n3 = 1; n2 = 0; n1 = 1; n0 = 1;
                 break;
   			case 'C' :  n3 = 1; n2 = 1; n1 = 0; n0 = 0;
                 break;
   			case 'D' :  n3 = 1; n2 = 1; n1 = 0; n0 = 1;
                 break;
   			case 'E' :  n3 = 1; n2 = 1; n1 = 1; n0 = 0;
                 break;
   			case 'F' :  n3 = 1; n2 = 1; n1 = 1; n0 = 1;
                 break;
        default : printf("\nError in HBconversion\n");
		}
		
	 	temp2[4*i] = n0;
	 	temp2[4*i+1] = n1;
	 	temp2[4*i+2] = n2;
	 	temp2[4*i+3] = n3;
	 }
 
 /* XOR function bit to bit*/
 for (i = 0; i < 4*imax; i++)
 {
  	 BITdistance[i] = temp1[i]^temp2[i];
 }
 
 /* Conversion back into hexadecimal */
 for(i=0; i < imax; i++)
 {
	 int n = 8*BITdistance[4*i+3] + 4*BITdistance[4*i+2] + 2*BITdistance[4*i+1] + BITdistance[4*i];
	 
 	 switch (n)
 	 {
	  		case 0 : HEXdistance[i] = '0';
	  			 break;
	  		case 1 : HEXdistance[i] = '1';
	  			 break;
	  		case 2 : HEXdistance[i] = '2';
	  			 break;
	  		case 3 : HEXdistance[i] = '3';
	  			 break;
	  		case 4 : HEXdistance[i] = '4';
	  			 break;
	  		case 5 : HEXdistance[i] = '5';
	  			 break;
	  		case 6 : HEXdistance[i] = '6';
	  			 break;
	  		case 7 : HEXdistance[i] = '7';
	  			 break;
	  		case 8 : HEXdistance[i] = '8';
	  			 break;
	  		case 9 : HEXdistance[i] = '9';
	  			 break;
	  		case 10 : HEXdistance[i] = 'A';
	  			 break;
	  		case 11 : HEXdistance[i] = 'B';
	  			 break;
	  		case 12 : HEXdistance[i] = 'C';
	  			 break;
	  		case 13 : HEXdistance[i] = 'D';
	  			 break;
	  		case 14 : HEXdistance[i] = 'E';
	  			 break;
	  		case 15 : HEXdistance[i] = 'F';
	  			 break;
		    default : printf("\nError in BHconversion");
		 }
	 }
	 return HEXdistance;
}
