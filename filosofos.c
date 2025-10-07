// filosofos.c (versión corregida)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

// Configuración por defecto
#define NUM_FILOSOFOS 3
#define TIEMPO_PENSAR_MIN 50000
#define TIEMPO_PENSAR_MAX 100000
#define TIEMPO_COMER_MIN 100000
#define TIEMPO_COMER_MAX 300000
#define TIEMPO_SIMULACION 30

// Variables globales dinámicas
sem_t *tenedores = NULL;
sem_t comedor;
pthread_mutex_t mutex_print;
int *filosofos_comiendo = NULL;
int *total_comidas = NULL;
volatile int ejecutando = 1;
int num_filosofos_global = NUM_FILOSOFOS;

// aleatorio thread-safe usando rand_r
long aleatorio_r(unsigned int *seedp, long min, long max) {
    if (max <= min) return min;
    return (rand_r(seedp) % (max - min + 1)) + min;
}

// VERSIÓN 1: CON INTERBLOQUEO
void* filosofo_naive(void* arg) {
    int id = *(int*)arg;
    free(arg);

    unsigned int seed = (unsigned int)time(NULL) ^ (unsigned int)(id * 2654435761u);

    while (ejecutando) {
        // Pensar
        pthread_mutex_lock(&mutex_print);
        printf("Filósofo %d está pensando...\n", id);
        pthread_mutex_unlock(&mutex_print);
        usleep(aleatorio_r(&seed, TIEMPO_PENSAR_MIN, TIEMPO_PENSAR_MAX));

        // Tomar tenedor izquierdo
        sem_wait(&tenedores[id]);
        pthread_mutex_lock(&mutex_print);
        printf("Filósofo %d tomó tenedor izquierdo (%d)\n", id, id);
        pthread_mutex_unlock(&mutex_print);

        // Tomar tenedor derecho
        sem_wait(&tenedores[(id + 1) % num_filosofos_global]);
        pthread_mutex_lock(&mutex_print);
        printf("Filósofo %d tomó tenedor derecho (%d)\n", id, (id + 1) % num_filosofos_global);
        filosofos_comiendo[id] = 1;
        total_comidas[id]++;
        pthread_mutex_unlock(&mutex_print);

        // Comer
        pthread_mutex_lock(&mutex_print);
        printf("Filósofo %d está COMIENDO\n", id);
        pthread_mutex_unlock(&mutex_print);
        usleep(aleatorio_r(&seed, TIEMPO_COMER_MIN, TIEMPO_COMER_MAX));

        // Soltar tenedores
        sem_post(&tenedores[id]);
        sem_post(&tenedores[(id + 1) % num_filosofos_global]);

        pthread_mutex_lock(&mutex_print);
        filosofos_comiendo[id] = 0;
        printf("Filósofo %d soltó los tenedores\n", id);
        pthread_mutex_unlock(&mutex_print);

        // Breve pausa antes de volver a pensar (evita hogging)
        usleep(1000);
    }
    return NULL;
}

// VERSIÓN 2: SIN INTERBLOQUEO (CON SEMÁFORO CONTADOR)
void* filosofo_semaforo(void* arg) {
    int id = *(int*)arg;
    free(arg);

    unsigned int seed = (unsigned int)time(NULL) ^ (unsigned int)(id * 2654435761u);

    while (ejecutando) {
        // Pensar
        pthread_mutex_lock(&mutex_print);
        printf("Filósofo %d está pensando...\n", id);
        pthread_mutex_unlock(&mutex_print);
        usleep(aleatorio_r(&seed, TIEMPO_PENSAR_MIN, TIEMPO_PENSAR_MAX));

        // Esperar turno en el comedor (máximo N-1 filósofos)
        sem_wait(&comedor);

        // Tomar tenedores
        sem_wait(&tenedores[id]);
        sem_wait(&tenedores[(id + 1) % num_filosofos_global]);

        pthread_mutex_lock(&mutex_print);
        printf("Filósofo %d tomó tenedores %d y %d\n", id, id, (id + 1) % num_filosofos_global);
        filosofos_comiendo[id] = 1;
        total_comidas[id]++;
        pthread_mutex_unlock(&mutex_print);

        // Comer
        pthread_mutex_lock(&mutex_print);
        printf("Filósofo %d está COMIENDO\n", id);
        pthread_mutex_unlock(&mutex_print);
        usleep(aleatorio_r(&seed, TIEMPO_COMER_MIN, TIEMPO_COMER_MAX));

        // Soltar tenedores
        sem_post(&tenedores[id]);
        sem_post(&tenedores[(id + 1) % num_filosofos_global]);

        // Liberar turno en el comedor
        sem_post(&comedor);

        pthread_mutex_lock(&mutex_print);
        filosofos_comiendo[id] = 0;
        printf("Filósofo %d soltó los tenedores\n", id);
        pthread_mutex_unlock(&mutex_print);

        // Breve pausa
        usleep(1000);
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    num_filosofos_global = NUM_FILOSOFOS;

    if (argc < 2) {
        printf("Uso: %s [naive|semaforo] [num_filosofos]\n", argv[0]);
        printf("Ejemplos:\n");
        printf("  %s naive           # 3 filósofos, versión ingenua\n", argv[0]);
        printf("  %s semaforo        # 3 filósofos, versión semáforo\n", argv[0]);
        printf("  %s semaforo 7      # 7 filósofos, versión semáforo\n", argv[0]);
        return 1;
    }

    // Leer número de filósofos si se especifica
    if (argc >= 3) {
        int n = atoi(argv[2]);
        if (n < 2) {
            printf("Error: Debe haber al menos 2 filósofos\n");
            return 1;
        }
        num_filosofos_global = n;
    }

    int usar_semaforo = 0;
    if (strcmp(argv[1], "semaforo") == 0) {
        usar_semaforo = 1;
        printf("=== MODO SEMÁFORO (SIN INTERBLOQUEO) ===\n");
    } else if (strcmp(argv[1], "naive") == 0) {
        printf("=== MODO INGENUO (CON INTERBLOQUEO) ===\n");
    } else {
        printf("Error: Opción no válida. Use 'naive' o 'semaforo'\n");
        return 1;
    }

    printf("Número de filósofos: %d\n", num_filosofos_global);

    // Reservar memoria dinámica para las estructuras
    tenedores = malloc(sizeof(sem_t) * num_filosofos_global);
    filosofos_comiendo = malloc(sizeof(int) * num_filosofos_global);
    total_comidas = malloc(sizeof(int) * num_filosofos_global);
    if (!tenedores || !filosofos_comiendo || !total_comidas) {
        perror("malloc");
        return 1;
    }

    pthread_t *filosofos = malloc(sizeof(pthread_t) * num_filosofos_global);
    if (!filosofos) { perror("malloc"); return 1; }

    // Inicializar semáforos y arrays
    for (int i = 0; i < num_filosofos_global; i++) {
        if (sem_init(&tenedores[i], 0, 1) != 0) {
            perror("sem_init tenedor");
            return 1;
        }
        filosofos_comiendo[i] = 0;
        total_comidas[i] = 0;
    }

    if (usar_semaforo) {
        if (sem_init(&comedor, 0, num_filosofos_global - 1) != 0) {
            perror("sem_init comedor");
            return 1;
        }
    }

    pthread_mutex_init(&mutex_print, NULL);

    // Crear hilos de filósofos
    for (int i = 0; i < num_filosofos_global; i++) {
        int *id = malloc(sizeof(int));
        if (!id) { perror("malloc id"); return 1; }
        *id = i;
        int rc;
        if (usar_semaforo) {
            rc = pthread_create(&filosofos[i], NULL, filosofo_semaforo, id);
        } else {
            rc = pthread_create(&filosofos[i], NULL, filosofo_naive, id);
        }
        if (rc != 0) {
            fprintf(stderr, "Error pthread_create para %d: %s\n", i, strerror(rc));
            return 1;
        }
    }

    // Ejecutar por tiempo determinado
    printf("Simulación ejecutándose por %d segundos...\n", TIEMPO_SIMULACION);

    for (int t = 0; t < TIEMPO_SIMULACION; t++) {
        sleep(1);
        pthread_mutex_lock(&mutex_print);
        printf("\n=== TIEMPO: %d segundos ===\n", t + 1);
        printf("Filósofos comiendo: ");
        for (int i = 0; i < num_filosofos_global; i++) {
            if (filosofos_comiendo[i]) {
                printf("%d ", i);
            }
        }
        printf("\nTotal comidas: ");
        for (int i = 0; i < num_filosofos_global; i++) {
            printf("F%d:%d ", i, total_comidas[i]);
        }
        printf("\n");
        pthread_mutex_unlock(&mutex_print);
    }

    // Terminar ejecución
    ejecutando = 0;
    printf("\nFinalizando simulación...\n");

    // Esperar a que terminen los hilos
    for (int i = 0; i < num_filosofos_global; i++) {
        pthread_join(filosofos[i], NULL);
    }

    // Resultados finales
    printf("\n=== RESULTADOS FINALES ===\n");
    printf("Total de comidas por filósofo:\n");
    for (int i = 0; i < num_filosofos_global; i++) {
        printf("Filósofo %d: %d comidas\n", i, total_comidas[i]);
    }

    // Liberar recursos
    for (int i = 0; i < num_filosofos_global; i++) {
        sem_destroy(&tenedores[i]);
    }
    if (usar_semaforo) {
        sem_destroy(&comedor);
    }
    pthread_mutex_destroy(&mutex_print);

    free(tenedores);
    free(filosofos_comiendo);
    free(total_comidas);
    free(filosofos);

    return 0;
}
