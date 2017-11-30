#include "projet.h"
#include <mpi.h>

/* 
 2017-03-26 : Version MPI
 Jules Chassetuillier
 Félix Ernest
 MAIN 4
 Polytech Paris UPMC
*/

/* permet le calcul du temps consacré au programme principal */
double my_gettimeofday(){
  struct timeval tmp_time;
  gettimeofday(&tmp_time, NULL);
  return tmp_time.tv_sec + (tmp_time.tv_usec * 1.0e-6L);
}

unsigned long long int node_searched = 0;

void evaluate(tree_t * T, result_t *result)
{
  node_searched++;
  
  move_t moves[MAX_MOVES];
  int n_moves;

  result->score = -MAX_SCORE - 1;
  result->pv_length = 0;
        
  if (test_draw_or_victory(T, result))
    return;

  if (TRANSPOSITION_TABLE && tt_lookup(T, result))     /* la réponse est-elle déjà connue ? */
    return;
        
  compute_attack_squares(T);

  /* profondeur max atteinte ? si oui, évaluation heuristique */
  if (T->depth == 0)
  {
    result->score = (2 * T->side - 1) * heuristic_evaluation(T);
    return;
  }
        
  n_moves = generate_legal_moves(T, &moves[0]);

  /* absence de coups légaux : pat ou mat */
  if (n_moves == 0)
  {
    result->score = check(T) ? -MAX_SCORE : CERTAIN_DRAW;
    return;
  }
        
  if (ALPHA_BETA_PRUNING)
    sort_moves(T, n_moves, moves);

  /* évalue récursivement les positions accessibles à partir d'ici */
  for (int i = 0; i < n_moves; i++)
  {
    tree_t child;
    result_t child_result;
                
    play_move(T, moves[i], &child);
                
    evaluate(&child, &child_result);
                         
    int child_score = -child_result.score;

    if (child_score > result->score)
    {
      result->score = child_score;
      result->best_move = moves[i];
      result->pv_length = child_result.pv_length + 1;
      
      for(int j = 0; j < child_result.pv_length; j++)
        result->PV[j+1] = child_result.PV[j];

      result->PV[0] = moves[i];
    }

    if (ALPHA_BETA_PRUNING && child_score >= T->beta)
      break;    

    T->alpha = MAX(T->alpha, child_score);
  }

  if (TRANSPOSITION_TABLE)
    tt_store(T, result);
}


void decide(tree_t * T, result_t *result, int rang, int np)
{ 
  int depth;
  int tag = 0; // tag normal est 0, le master le met à 1 quand un des proc a trouvé
  int score, temp;
  int compteur = 0;
  MPI_Status status;
  
  if (rang==0)
  {
  printf("Je suis le maitre (rang = %d)\n", rang);
    for (depth = 1;; depth++) 
    {
      if(depth < np)// Début : parcours des processeurs tout en modifiant la profondeur (d'où target = depth)
      {
        MPI_Send(&depth,1,MPI_INT, depth, tag, MPI_COMM_WORLD);
printf("MASTER : J'ai envoyé un message initial à P%d\n", depth);
      }
      else
      {
        // Réception des travaux terminés
        MPI_Recv(&temp,1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        score = temp;
printf("MASTER : J'ai reçu un message de P%d dont le contenu est score = %d\n", status.MPI_SOURCE, score);        
    
        if(tag == 0 && status.MPI_TAG == 0) // le processeur n'a pas trouvé de résultat
        {
          MPI_Send(&depth,1,MPI_INT, status.MPI_SOURCE, tag, MPI_COMM_WORLD);
printf("MASTER : J'ai répondu par un message à P%d pour qu'il fasse un autre travail (depth = %d)\n", status.MPI_SOURCE, depth);        
        }
        else // résultat trouvé : il répond en demandant l'arrêt des calculs
        {
          tag = 1;
          MPI_Send(&depth, 1, MPI_INT, status.MPI_SOURCE, tag, MPI_COMM_WORLD);
          compteur++;
printf("MASTER : J'ai répondu par un message à P%d pour qu'il ne fasse pas un autre travail (car score = %d)\n", status.MPI_SOURCE, score);       
        }
    


        printf("=====================================\n");
        printf("depth: %d / score: %.2f / best_move : ", depth, 0.01 * score);
        //print_pv(T, result);
             
        if (compteur == np-1) // dans ce cas, tous les processeurs ont reçu un message avec tag = 1 (i.e. tous les processeurs savent qu'il n'y a plus de travail à faire)
          break; 
      }
    }
  }
  else
  {
    // Reception initiale
    MPI_Recv(&depth, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
    
    while(status.MPI_TAG == 0)
    {     
printf("Je suis P%d et j'ai reçu depth = %d avec tag = %d\n", rang, depth, status.MPI_TAG);
      
      // affectation de l'arbre avec la valeur reçue 
      T->depth = depth;
      T->height = 0;
      T->alpha_start = T->alpha = -MAX_SCORE - 1;
      T->beta = MAX_SCORE + 1;
    
      evaluate(T, result);
    
      if (DEFINITIVE(result->score)) // dans ce cas, le résultat final est trouvé alors tag = 1
      {
        MPI_Send(&(result->score), 1, MPI_INT, 0, 1, MPI_COMM_WORLD);
printf("Je suis P%d et j'ai envoyé score = %d comme résultat définitif (depth = %d)\n", rang, result->score, depth);
      } 
      else // le résultat n'est pas définitif
      {
        MPI_Send(&(result->score), 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
printf("Je suis P%d et j'ai envoyé score = %d (pour depth = %d)\n", rang, result->score, depth);
      }
      
      // Reception du travail suivant de la part du maitre
      MPI_Recv(&depth, 1, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
    }
  }
}

int main(int argc, char **argv)
{
  double debut, fin;
  tree_t root;
  result_t result;
  
  // Variables MPI
  int rang, np, tag;
  

    if (argc < 2) {
          printf("usage: %s \"4k//4K/4P w\" (or any position in FEN)\n", argv[0]);
          exit(1);
    }

    if (ALPHA_BETA_PRUNING)
          printf("Alpha-beta pruning ENABLED\n");

    if (TRANSPOSITION_TABLE) {
          printf("Transposition table ENABLED\n");
          init_tt();
    }
        
    parse_FEN(argv[1], &root);
    print_position(&root);
    
    // Initialisation MPI
    MPI_Init(&argc, &argv); 
  MPI_Comm_size(MPI_COMM_WORLD, &np);
  MPI_Comm_rank(MPI_COMM_WORLD, &rang);
            
    debut = my_gettimeofday();
    
  decide(&root, &result, rang, np);
  
  fin = my_gettimeofday();
  
  MPI_Finalize();

  printf("\nDécision de la position: ");
    
    switch(result.score * (2*root.side - 1)) {
        case MAX_SCORE: printf("blanc gagne\n"); break;
        case CERTAIN_DRAW: printf("partie nulle\n"); break;
        case -MAX_SCORE: printf("noir gagne\n"); break;
        default: printf("BUG\n");
    }

    printf("Node searched: %llu\n", node_searched);
    
    printf("Computing time : %lf s\n", fin - debut);
        
    if (TRANSPOSITION_TABLE)
          free_tt();
  
  return 0;
}