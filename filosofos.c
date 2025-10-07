#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

// Configuración por defecto
#define NUM_FILOSOFOS 5
#define TIEMPO_PENSAR_MIN 1000000
#define TIEMPO_PENSAR_MAX 3000000
#define TIEMPO_COMER_MIN 500000
#define TIEMPO_COMER_MAX 1500000
#define TIEMPO_SIMULACION 30

// Variables globales
sem_t tenedores[NUM_FILOSOFOS];
sem_t comedor;
pthread_mutex_t mutex_print;
int filosofos_comiendo[NUM_FILOSOFOS];
int total_comidas[NUM_FILOSOFOS];
int ejecutando = 1;

// Función para generar números aleatorios
long aleatorio(long min, long max) {
    return (rand() % (max - min + 1)) + min;
}

// VERSIÓN 1: CON INTERBLOQUEO
void* filosofo_naive(void* arg) {
    int id = *(int*)arg;
    
    while (ejecutando) {
        // Pensar
        pthread_mutex_lock(&mutex_print);
        printf("Filósofo %d está pensando...\n", id);
        pthread_mutex_unlock(&mutex_print);
        usleep(aleatorio(TIEMPO_PENSAR_MIN, TIEMPO_PENSAR_MAX));
        
        // Tomar tenedor izquierdo
        sem_wait(&tenedores[id]);
        pthread_mutex_lock(&mutex_print);
        printf("Filósofo %d tomó tenedor izquierdo (%d)\n", id, id);
        pthread_mutex_unlock(&mutex_print);
        
        // Tomar tenedor derecho
        sem_wait(&tenedores[(id + 1) % NUM_FILOSOFOS]);
        pthread_mutex_lock(&mutex_print);
        printf("Filósofo %d tomó tenedor derecho (%d)\n", id, (id + 1) % NUM_FILOSOFOS);
        filosofos_comiendo[id] = 1;
        total_comidas[id]++;
        pthread_mutex_unlock(&mutex_print);
        
        // Comer
        pthread_mutex_lock(&mutex_print);
        printf("Filósofo %d está COMIENDO\n", id);
        pthread_mutex_unlock(&mutex_print);
        usleep(aleatorio(TIEMPO_COMER_MIN, TIEMPO_COMER_MAX));
        
        // Soltar tenedores
        sem_post(&tenedores[id]);
        sem_post(&tenedores[(id + 1) % NUM_FILOSOFOS]);
        filosofos_comiendo[id] = 0;
        
        pthread_mutex_lock(&mutex_print);
        printf("Filósofo %d soltó los tenedores\n", id);
        pthread_mutex_unlock(&mutex_print);
    }
    
    free(arg);
    return NULL;
}

// VERSIÓN 2: SIN INTERBLOQUEO (CON SEMÁFORO CONTADOR)
void* filosofo_semaforo(void* arg) {
    int id = *(int*)arg;
    
    while (ejecutando) {
        // Pensar
        pthread_mutex_lock(&mutex_print);
        printf("Filósofo %d está pensando...\n", id);
        pthread_mutex_unlock(&mutex_print);
        usleep(aleatorio(TIEMPO_PENSAR_MIN, TIEMPO_PENSAR_MAX));
        
        // Esperar turno en el comedor (máximo N-1 filósofos)
        sem_wait(&comedor);
        
        // Tomar tenedores
        sem_wait(&tenedores[id]);
        sem_wait(&tenedores[(id + 1) % NUM_FILOSOFOS]);
        
        pthread_mutex_lock(&mutex_print);
        printf("Filósofo %d tomó tenedores %d y %d\n", id, id, (id + 1) % NUM_FILOSOFOS);
        filosofos_comiendo[id] = 1;
        total_comidas[id]++;
        pthread_mutex_unlock(&mutex_print);
        
        // Comer
        pthread_mutex_lock(&mutex_print);
        printf("Filósofo %d está COMIENDO\n", id);
        pthread_mutex_unlock(&mutex_print);
        usleep(aleatorio(TIEMPO_COMER_MIN, TIEMPO_COMER_MAX));
        
        // Soltar tenedores
        sem_post(&tenedores[id]);
        sem_post(&tenedores[(id + 1) % NUM_FILOSOFOS]);
        filosofos_comiendo[id] = 0;
        
        // Liberar turno en el comedor
        sem_post(&comedor);
        
        pthread_mutex_lock(&mutex_print);
        printf("Filósofo %d soltó los tenedores\n", id);
        pthread_mutex_unlock(&mutex_print);
    }
    
    free(arg);
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Uso: %s [naive|semaforo]\n", argv[0]);
        printf("  naive:    Versión con posible interbloqueo\n");
        printf("  semaforo: Versión sin interbloqueo\n");
        return 1;
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
    
    pthread_t filosofos[NUM_FILOSOFOS];
    
    // Inicializar semáforos y mutex
    for (int i = 0; i < NUM_FILOSOFOS; i++) {
        sem_init(&tenedores[i], 0, 1);
        filosofos_comiendo[i] = 0;
        total_comidas[i] = 0;
    }
    
    if (usar_semaforo) {
        sem_init(&comedor, 0, NUM_FILOSOFOS - 1);
    }
    
    pthread_mutex_init(&mutex_print, NULL);
    srand(time(NULL));
    
    // Crear hilos de filósofos
    for (int i = 0; i < NUM_FILOSOFOS; i++) {
        int* id = malloc(sizeof(int));
        *id = i;
        if (usar_semaforo) {
            pthread_create(&filosofos[i], NULL, filosofo_semaforo, id);
        } else {
            pthread_create(&filosofos[i], NULL, filosofo_naive, id);
        }
    }
    
    // Ejecutar por tiempo determinado
    printf("Simulación ejecutándose por %d segundos...\n", TIEMPO_SIMULACION);
    
    for (int t = 0; t < TIEMPO_SIMULACION; t++) {
        sleep(1);
        pthread_mutex_lock(&mutex_print);
        printf("\n=== TIEMPO: %d segundos ===\n", t + 1);
        printf("Filósofos comiendo: ");
        for (int i = 0; i < NUM_FILOSOFOS; i++) {
            if (filosofos_comiendo[i]) {
                printf("%d ", i);
            }
        }
        printf("\nTotal comidas: ");
        for (int i = 0; i < NUM_FILOSOFOS; i++) {
            printf("F%d:%d ", i, total_comidas[i]);
        }
        printf("\n");
        pthread_mutex_unlock(&mutex_print);
    }
    
    // Terminar ejecución
    ejecutando = 0;
    printf("\nFinalizando simulación...\n");
    
    // Esperar a que terminen los hilos
    for (int i = 0; i < NUM_FILOSOFOS; i++) {
        pthread_join(filosofos[i], NULL);
    }
    
    // Resultados finales
    printf("\n=== RESULTADOS FINALES ===\n");
    printf("Total de comidas por filósofo:\n");
    for (int i = 0; i < NUM_FILOSOFOS; i++) {
        printf("Filósofo %d: %d comidas\n", i, total_comidas[i]);
    }
    
    // Liberar recursos
    for (int i = 0; i < NUM_FILOSOFOS; i++) {
        sem_destroy(&tenedores[i]);
    }
    if (usar_semaforo) {
        sem_destroy(&comedor);
    }
    pthread_mutex_destroy(&mutex_print);
    
    return 0;
}