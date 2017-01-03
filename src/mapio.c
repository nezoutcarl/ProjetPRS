#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "map.h"
#include "error.h"

#ifdef PADAWAN

/**
 *  
 * Get nextline (until \n) and return the line
 * @param fd Open file descriptor for reading
 * @return A malloced string \0 terminated
 */
char* getLine(int fd) {
    char buffer[1024];
    buffer[0] = '\0';
    int i = 0;
    int n = 0;
    while (i < 1024 && (n = read(fd, buffer + i, 1)) > 0) {
        if (buffer[i] == '\n') {
            buffer[i] = '\0';
            break;
        }
        i += n;
    }
    char *buff = malloc(sizeof (char)*(i + 1));
    memcpy(buff, buffer, sizeof (char)*(i + 1));
    return buff;
}

/**
 * Create and init a new map
 * @param width of the new map
 * @param height of the new map
 */
void map_new(unsigned width, unsigned height) {
    map_allocate(width, height);

    for (int x = 0; x < width; x++)
        map_set(x, height - 1, 0); // Ground

    for (int y = 0; y < height - 1; y++) {
        map_set(0, y, 1); // Wall
        map_set(width - 1, y, 1); // Wall
    }

    map_object_begin(6);

    // Texture pour le sol
    map_object_add("images/ground.png", 1, MAP_OBJECT_SOLID);
    // Mur
    map_object_add("images/wall.png", 1, MAP_OBJECT_SOLID);
    // Gazon
    map_object_add("images/grass.png", 1, MAP_OBJECT_SEMI_SOLID);
    // Marbre
    map_object_add("images/marble.png", 1, MAP_OBJECT_SOLID | MAP_OBJECT_DESTRUCTIBLE);
    map_object_add("images/flower.png", 1, MAP_OBJECT_AIR);
    map_object_add("images/coin.png", 20, MAP_OBJECT_COLLECTIBLE);
    map_object_end();

}
/**
 * Save current map to filename
 * @param filename
 */
void map_save(char *filename) {
    int file = open(filename, O_CREAT | O_TRUNC | O_WRONLY, 0666); //on met le fichier dans un descripteur 
    /* test sur l'ouverture */
    if (file == -1) {
        fprintf(stderr, "Sorry: Save file can't be create\n");
        return;
    }
    int save_stdout = dup(1);
    if (save_stdout == -1) {
        fprintf(stderr, "Sorry: Can't backup stdout\n");
        return;
    }
    if (dup2(file, 1) == -1) {
        fprintf(stderr, "Sorry: Can't duplicate file to stdout\n");
        return;
    }
    /* récupération et stockage dans le fichier en entier non signé de la largeur/hauteur/nombre d'objets */
    unsigned int width = map_width();
    unsigned int height = map_height();
    write(file,&width,sizeof(unsigned int));
    write(file,&height,sizeof(unsigned int));
    unsigned int nb_obj = map_objects();
    write(file,&nb_obj,sizeof(unsigned int));
    printf("\n");
    /* on écrit chaque objet et ses caractéristiques dans le fichier ligne ligne par ligne étant donné que la sortie standard correspond au fichier */
    for (int i = 0; i < nb_obj; i++) {
        printf("%s\t", map_get_name(i));
        printf("%d\t", map_get_frames(i));
        printf("%d\t", map_get_solidity(i));
        printf("%d\t", map_is_destructible(i));
        printf("%d\t", map_is_collectible(i));
        printf("%d\n", map_is_generator(i));
    }
    /* sauvegarde des objets actuellement sur la carte */
    int OBJECT_TYPE;
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++) {
            if ((OBJECT_TYPE = map_get(x, y)) != MAP_OBJECT_NONE) {
                printf("%d\t%d\t", x, y);
                printf("%d\n", OBJECT_TYPE);
            }
        }
    printf("END\n"); // fin d'écriture de ces objets
    fflush(stdout);
    close(file);
    dup2(save_stdout, 1); //réinitialisation de la sortie standard
    printf("Maps saved !");
}
/**
 * Load the map located at filename
 * @param filename
 */
void map_load(char *filename) {
    /* ouverture et vérification sur le fichier */
    int file = open(filename, O_RDONLY);
    if (file == -1) {
        fprintf(stderr, "Sorry: Save file can't be load\n");
        return;
    }
    /* on récupère les les 3 données importantes de bases, largeur/hauteur et nombre d'objets "différents"*/
    unsigned int width,height,nb_obj;
    read(file,&width,sizeof(int));
    read(file,&height,sizeof(int));
    read(file,&nb_obj,sizeof(int));
    /* on alloue la dimension de notre carte */
    map_allocate(width, height);
    /*mise en place des différents objets de la carte */
    map_object_begin(nb_obj);
    /* on revient au début du fichier puis on libère la première ligne contenant les données de dimensions pour la carte */
    lseek(file,0,SEEK_SET);
    free(getLine(file));
    char *line;
    for (int i = 0; i < nb_obj; i++) {
        line = getLine(file);
	/* on récupère les données en chaine en faisant un split via strtok */ 
        char *path = strtok(line, "\t");
        char *nb_frame = strtok(NULL, "\t");
        char *solidity = strtok(NULL, "\t");
        char *destruct = strtok(NULL, "\t");
        char *collect = strtok(NULL, "\t");
        char *generator = strtok(NULL, "\t");
	/* on convertit ces données en entier */
        int i_nb_frame = atoi(nb_frame);
        int i_solidity = atoi(solidity);
        int i_destruct = atoi(destruct);
        int i_collect = atoi(collect);
        int i_generator = atoi(generator);
        int flags = 0;
	/* on cumule les différentes caractéristiques de l'objet */
        flags |= i_solidity;
        if(i_destruct) {
            flags |= MAP_OBJECT_DESTRUCTIBLE;
        }
        if(i_collect) {
            flags |= MAP_OBJECT_COLLECTIBLE;
        }
        if(i_generator) {
            flags |= MAP_OBJECT_GENERATOR;
        }
        /* ajout de l'obet et on désaloue la ligne traitée */
        map_object_add(path, i_nb_frame, flags);
        free(line);
    }
    map_object_end(); //fin d'ajout des différents objets disponibles pour la carte
    /* récupération des objets présents sur la carte */
    line = getLine(file);//récupération de la ligne suivante
    while(strcmp(line,"END") != 0) {
        printf("%s\n",line);
	/* segmentation de la ligne en plusieurs chaines */
        char *x = strtok(line, "\t");
        char *y = strtok(NULL, "\t");
        char *obj = strtok(NULL, "\t");
	/* conversion en entier */
        int i_x = atoi(x);
        int i_y = atoi(y);
        int i_obj = atoi(obj);
	/* mise en place sur la carte de l'objet */
        map_set(i_x,i_y,i_obj);
        free(line);// désalocation de la ligne 
        line = getLine(file); //ligne suivante jusqu'à arriver à la fin du fichier reconnue par la chaine END
    }
    close(file);


    //exit_with_error("Map load is not yet implemented\n");
}

#endif
