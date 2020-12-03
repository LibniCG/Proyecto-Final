#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <curses.h>
#include <sys/stat.h>
#include <sys/mman.h>

#define NUMSECTORS 63
#define NUMHEADS 255
#define TAMSECTOR 512 
#define MiB 1048000


/*** Declaración de variables globales ****/
int fs;
int fd;
char *map;
int lineas;
int countModif = 0;

typedef struct datos { //Datos de la imagen
    short int *sector; //Tamaño del sector
    int spc; //Sectores por cluster
    short int *sr; //Sectores reservados
    int copias; //Número de copias del FAT
    short int *re; //Entradas del directorio raíz
    int *sd; //Número de sectores del disco
    short int *tam; //Tamaño del FAT
    char ev[11]; //Etiqueta volumen
    char id[5]; //ID Sistema
} INFOIMG;

/*** Declaración de funciones ****/
char *mapFile(char *filePath);
int getNext(int cluster, int base);
int esMBR(char *base);
void pruebas();
void getInfo();
void getInfoMBR(char *filename);
void getInfoParticion(int inicio, char *filename);
void getInfoDirectorio();
void openF (char *filename);
int leeChar();
char *hazLinea(char *base, unsigned long long dir);

int getIntoPart();
int leerdatos(int i);


int main(int argc, char *argv[]){
    openF(argv[1]);
}

char *mapFile(char *filePath){

    fd = open(filePath, O_RDWR);
    if (fd == -1){
        perror("Error");
        return(NULL);
    }
    
    struct stat st;
    fstat(fd, &st);
    fs = st.st_size;

    char *mapF = mmap(0, fs+200, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapF == MAP_FAILED) {
        close(fd);
        perror("Error mapeando el archivo");
        return(NULL);
    }
    return mapF;
}

int getNext(int cluster, int base) {
	// Para FAT12
	int offset = cluster + cluster/2;
	int flag = cluster % 2; // Nos dice si en la parte baja o alta

	unsigned char b1,b2;
      //printf("%x", map);
	b1 = map[base+offset];
	//printf("%02x:",b1);
	b2 = map[base+offset+1];
	//printf("%02x\n",b2);
	int result = b1 | b2<<8; // Los bits mas significativos van al final

	if(flag) {
		result >>= 4;
	}
	else {
		result &= 0xfff;
	}

	//printf("%04x\n",result);
  
	return result;
}

void pruebas(){
    int d;
    for(int i = 0; i<10; i++){
        d = getNext (i,0x200);
        printf("%03x %d\n",d,d);
    }
}

void openF (char *filename){

    map = mapFile(filename);
    
    if (map ==NULL){
      exit(EXIT_FAILURE);
    }
    
    //pruebas();
    if(esMBR(map)){
        getInfoMBR(filename);
    }else{
        getInfo();
    }
     
    if (munmap(map,fs) == -1){
        perror("Error");
    }
    
    close(fs);
}

void getInfo(){

    INFOIMG info;

    printf("\n\n\t****** BPB (BIOS Parameter Block) ******\n");

    info.sector = (short int *)&map[11]; //Tamaño sector
    printf("\tTamaño sector                %d \n", *info.sector); ///int aux = info.sector;
    
    info.spc =  map[13]; //Sectores por cluster
    printf("\tSectores por cluster         %d \n", info.spc);
    
    info.sr = (short int *)&map[14]; //Sectores reservados
    printf("\tSectores por reservados      %d \n", *info.sr);
    
    info.copias = map[16]; //Num de copias del FAT
    printf("\tNumero de copias del FAT     %d \n", info.copias);
    
    info.re = (short int *)&map[17]; //Entadas del directorio raíz
    printf("\tEntradas directorio raiz     %d \n", *info.re);
    
    info.sd = (int *)&map[32]; //Num sectores del disco
    printf("\tNumero sectores del disco    %d \n", *info.sd);

    info.tam = (short int *)&map[22]; //Tamaño del FAT
    printf("\tTamaño del FAT               %d \n", info.tam);
    
    strcpy(info.ev, &map[43]); //Etiqueta de volumen
    printf("\tEtiqueta de volumen          %s \n", info.ev);

    strncpy(info.id, &map[0x36],5); //ID del Sistema
    printf("\tId Sistema                   %s \n\n", info.id);

    /*int dirRaiz = (*info.sr + (info.tam * info.copias)) * (*info.sector);
    printf("\tDirectorio Raíz              0x%04x\n", dirRaiz);   

    int datos = dirRaiz + ((*info.re) * 32);
    printf("\tInicio de Datos              0x%04x\n\n", datos); */
}

void getInfoMBR(char *filename){
    int c; //Cilindro
    int h; //Cabeza
    int s; //Sector
    int inicio, fin, tipo, *tam, j;
    int prDatas [4][4], prInicio [4];
    int cur = 0;

    //CURSES comienza

    initscr();
    raw();
    noecho(); /* No muestres el caracter leido */
    cbreak(); /* Haz que los caracteres se le pasen al usuario */

    move(1,0);
    printw("\n\tParticion\tBoot\tInicio\t  Fin\tTamaño\t   Tipo\n\n");

     //Elegir particion
    do {
        for(int i = 0; i < 4; i++) {

            if (i == 4) attron(A_REVERSE);

            h = (unsigned char)map[0x1BE + 1 + (i * 16)];
            if (h != 0) { //Verificar que haya información
                c = map[0x1BE + 2 + (i*16)] & 0xC0; c<<=2; c |= map[0x1BE + 3 + (i * 16)];
                s = map[0x1BE + 2 + (i * 16)] & 0x3F;
                inicio = ((c * NUMHEADS + h) * NUMSECTORS + (s - 1)) * TAMSECTOR;
                //printf("\n Incio de la partición: %02x\n\n", inicio);

                c = map[0x1BE + 6 + (i * 16)] & 0xC0; c<<=2; c |= map[0x1BE + 7 + (i * 16)];
                h = (unsigned char) map[0x1BE + 5 + (i * 16)]; 
                s = map[0x1BE + 6 + (i * 16)] & 0x3F;

                //printf("\n c: %d, h: %d, s: %d",c,h,s);
                fin = (c * NUMHEADS + h) * NUMSECTORS + (s - 1);

                tam = (int *)&map[0x1BE + 12 + (i * 16)]; 
                int t = *tam; t = (t * TAMSECTOR) / MiB;

                tipo = map[0x01BE + 4 + (i * 16)];

                prDatas [i][0] = inicio/512;
                prDatas [i][1] = fin;
                prDatas [i][2] = t;
                prDatas [i][3] = tipo;

                prInicio[i] = inicio;

                move(4+i,0);
                printw("\tParticion %d\t\t%d\t%d\t%d MiB\t    %d\n", i + 1, prDatas[i][0], prDatas[i][1], prDatas[i][2], prDatas[i][3]);
            } else{
                prDatas [i][0] = 0;
                prDatas [i][1] = 0;
                prDatas [i][2] = 0;
                prDatas [i][3] = 0;

                prInicio[i] = 0;

                move(4+i,0);
                printw("\tParticion %d\t\t%d\t%d\t  %d MiB\t    %d\n", i + 1, prDatas[i][0], prDatas[i][1], prDatas[i][2], prDatas[i][3]);
            }
            attroff(A_REVERSE);
        }

        move(4+cur,8);
        refresh();
        c = leeChar();
      
        switch(c) {

            case 0x1B5B41: // ARRIBA
                cur = (cur > 0) ? cur - 1 : 4-1;
                break;
            case 0x1B5B42: // ABAJO
                cur = (cur < 4-1) ? cur + 1 : 0;
                break;
            case 0xA:
                move(0,0);
            
                j = cur;

                break; //Fin case ENTER
            default:
                // Nothing 
                break;
        }
        //move(15,5);
        //printw("Estoy en %d:",cur);
      
    } while (c != 0xA);

    endwin();
    
    //Este código se manda a llamar cuando seleccione una partición y se debe mandar el inicio de la partición que eligió  
    
    h = (unsigned char)map[prInicio[j] + 0x1BE + 1]; //Verficar si hay una partición extendida
    
    if(h != 0 && prInicio[j] != 0) {
        printf("\n\tPartición Extendida\n"); 
        s = map[prInicio[j] + 0x1BE + 2] & 0x3F;
        c = map[prInicio[j] + 0x1BE + 2] & 0xC0; c<<=2; c |= map[prInicio[j] + 0x1BE + 3];
        prInicio[j] = ((c * NUMHEADS + h) * NUMSECTORS + (s - 1)) * TAMSECTOR;
        //printf("Partition Extended Start: %02x\n\n", inicio);
        getInfoParticion(prInicio[j], filename);
    }else{ 
        getInfoParticion(prInicio[j], filename);
    }
}

void getInfoParticion(int inicio, char *filename){
    INFOIMG info;
    
    if(inicio != 0) {
        printf("\n\tImagen: Inicio de Particion %02x / %s\n\n", inicio, filename);

        info.sector = (short int *)&map[inicio + 11]; //Tamaño sector
        printf("\tTamaño sector                %d \n", *info.sector);
     
        info.sr = (short int *)&map[inicio + 14]; //Sectores reservados
        printf("\tSectores por reservados      %d \n", *info.sr);

        info.copias = map[inicio + 16]; //Num de copias del FAT
        printf("\tNumero de copias del FAT     %d \n", info.copias);

        info.re = (short int *)&map[inicio + 17]; //Entadas del directorio raíz
        printf("\tEntradas directorio raiz     %d \n", *info.re);

        info.tam = (short int *)&map[inicio + 22]; //Tamaño del FAT
        printf("\tTamaño del FAT               %d \n", *info.tam);

        info.sd = (int *)&map[inicio + 32]; //Num sectores del disco
        printf("\tNumero sectores del disco    %d \n", *info.sd);

        strncpy(info.ev, &map[inicio + 43], 10); //Etiqueta de volumen
        printf("\tEtiqueta de volumen          %s \n", info.ev);

        strncpy(info.id, &map[inicio + 0x36], 5); //ID del Sistema
        printf("\tId Sistema                   %s \n\n", info.id); 

        /*int dirRaiz = (*info.sr + (info.tam * info.copias)) * (*info.sector);
        printf("\tDirectorio Raíz              0x%04x\n", dirRaiz); 

        int datos = dirRaiz + ((*info.re) * 32);
        printf("\tInicio de Datos              0x%04x\n\n", datos); */
    } else {
        printf("\n\tEsta partición está vacía. \n\n");
    }
}

void getInfoDirectorio(){
    /*int i; 
    int tipo, cluster, tam; 
    char nombre[15] = "X";

    printf("\t Nombre\t Tipo\t Cluster\t Tamaño\n\n");
    for(int i = 0; i < 4; i ++){
        strcpy(nombre);
        tipo = map[12 + i * 32];
        cluster = *(short int *)&map[0x1a + i * 32];
        tam =  *((int *)&map[0x1c + i * 32]);
        printf("\t %s\t %d\t %d\t %d\n", nombre, tipo, cluster, tam);
    }*/
}

int getArchivo(char *filename){
    /*char *map = mapFile(filename);
    int c2;
    int x = 0;
    int y = 0;
    int px = 0;
    int offsetArch = 0;

    countModif=0;

    if (map == NULL) {
        exit(EXIT_FAILURE);
    }

    do {
        for(int i= 0; i<25; i++) {
            // Haz linea, base y offset
            char *l = hazLinea(map,(unsigned long long)(i+offsetArch)*16);
            mvprintw(i,0,l);
        }
        //mvprintw(27,0,"X: %d Y: %d FS: %d Mod: %d", x, y+offsetArch, fs, countModif*sizeof(char));
        refresh();
        px = (x<16) ? x*3 : 32+x;
        move(0+y, 9+px);
        c2 = getch();

        switch(c2) {
            case KEY_UP:
                if(y == 0) {
                    if(offsetArch != 0) {
                        offsetArch--;
                    }
                } else {
                    y-=1;
                }
                break;
            case KEY_DOWN:      
                if(y == 24) {
                    if((y+offsetArch) >= lineas) {
                    } else {
                        offsetArch++;
                    }
                } else {
                    y+=1;
                }
                break;
            case KEY_LEFT:
                if(x > 0) {
                    x-=1;
                }
                break;
            case KEY_RIGHT:
                if (x < 31) {
                    x+=1;
                } else {
                    x = 0; y += 1;
                }
                break;
            default:
            break;
    } while(c2!=24);
    close(fd);
    clear();
    refresh();*/
}

int esMBR(char *base) {
    int res = 1;
    int i = 0;

    if(base[510] != 0x55 && base[511] != 0xAA){
        res = 0;
    }
    while(res && i < 4){
        int numPart = 0x1BE + i * 16;
        if(!(base[numPart] == 0 || base[numPart] == 0x80)) {
            res = 0;
        }
        i++;
    }
    return res;
}

int leeChar() {
  int chars[5];
  int ch,i=0;
  nodelay(stdscr, TRUE);
  while((ch = getch()) == ERR); /* Espera activa */
  ungetch(ch);
  while((ch = getch()) != ERR) {
    chars[i++]=ch;
  }
  /* convierte a numero con todo lo leido */
  int res=0;
  for(int j=0;j<i;j++) {
    res <<=8;
    res |= chars[j];
  }
  return res;
}

char *hazLinea(char *base, unsigned long long dir) {
	// mvprintw(27,0,"%d", lineas);
	char linea[100]; // La linea es mas pequeña
	int o=0;
	// Muestra 16 caracteres por cada linea
	o += sprintf(linea,"%08x ",dir); // offset en hexadecimal
	for(int i=0; i < 4; i++) {
		unsigned char a,b,c,d;
		a = base[dir+4*i+0];
		b = base[dir+4*i+1];
		c = base[dir+4*i+2];
		d = base[dir+4*i+3];
		o += sprintf(&linea[o],"%02x %02x %02x %02x ", a, b, c, d);
	}
	for(int i=0; i < 16; i++) {
		if (isprint(base[dir+i])) {
			o += sprintf(&linea[o],"%c",base[dir+i]);
		}
		else {
			o += sprintf(&linea[o],".");
		}
	}
	sprintf(&linea[o],"\n");

	return(strdup(linea));
}