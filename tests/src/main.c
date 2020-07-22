

#include "../include/main.h"
#include "../include/player.h"

#define S_WIDTH  700
#define S_HEIGHT 700
#define L_WIDTH  3000
#define L_HEIGHT S_HEIGHT

struct entity_t *player;
struct stage_t   stage;

static SDL_Rect                  screen_edge;
static struct background_t *     bg;
static struct fade_color_t       f;
static struct particle_system_t *ps;
static struct grid_t *           grid;

static void init_scene( void );
static void cleanup_stage( void );
static void draw( void );
static void tick( void );
static void check_enemy_collision( void );
static void add_particles( int32_t, int32_t, size_t );

static void update_trails( void );
static void update_enemies( void );
static void update_parallax_backgrounds( void );

static void draw_trails( void );
static void draw_enemies( void );
static void draw_parallax_backgrounds( void );

/**
 * Barebones game. This is the minimum amount of code
 * necessary to run a window.
 *
 * @param int argc, number of cmd arguments.
 * @param char* [] array of string arguments.
 *
 * @return status code.
 */
int
main( int argc, char *argv[] ) {
  Stds_InitGame( "Trail, Parallax Test, and Button Test", S_WIDTH, S_HEIGHT, L_WIDTH, L_HEIGHT );
  Stds_InitAppStructures();
  init_scene();
  Stds_GameLoop();

  atexit( cleanup_stage );
  return 0;
}

/**
 * Initializes the delegation structure to use the
 * tick and draw methods for rendering and updating
 * the game. These methods must be defined because the
 * delegation structure uses them in the loop function.
 *
 * Sprites and structs are also initalized here.
 */
static void
init_scene( void ) {
  app.delegate.tick = tick;
  app.delegate.draw = draw;
  stage.enemy_tail  = &stage.enemy_head;

  init_player();

  /* Create the green grass tiles for collision testing. */
  for ( uint32_t i = 0, x = 0; i < 30; i++, x += 48 ) {
    struct entity_t *e;
    e = add_enemy( x, app.LEVEL_HEIGHT - 20 );

    stage.enemy_tail->next = e;
    stage.enemy_tail       = e;
  }

  /* Initialize the parallax background with each frame's scroll factor. */
  uint8_t parallax_frames = 11;

  float parallax_scroll[11] = {0.10f, 0.15f, 0.20f, 0.25f, 0.30f, 0.35f,
                               0.40f, 0.45f, 0.50f, 0.55f, 0.60f};
  Stds_AddParallaxBackground( "tests/res/img/background_4/layer_0", parallax_frames, 1.0f,
                              parallax_scroll, false );

  /* Create the border fade from blue to yellow. */
  SDL_Color c1 = {0xff, 0xff, 0, 0xff};
  SDL_Color c2 = {0, 0, 0xff, 0xff};

  f.c1    = c1;
  f.c2    = c2;
  f.time  = 0.0f;
  f.alpha = 0.01f;

  /* Generate a standard particle system. */
  ps = Stds_CreateParticleSystem( 2048 );

  /* Create the white grid for testing. */
  SDL_Color tempGridColor = {255, 255, 255, 255};
  grid                    = Stds_CreateGrid( 0, 0, 32, 32, 10, 10, tempGridColor, tempGridColor );
}

/*
 *
 */
static void
tick( void ) {
  if ( app.mouse.button[SDL_BUTTON_LEFT] ) {
    add_particles( app.mouse.x, app.mouse.y, 128 );
  }

  Stds_CameraUpdate( player );
  Stds_ParticleSystemUpdate( ps );
  update_parallax_backgrounds();
  update_trails();
  update_enemies();
  player_update();
}

/*
 *
 */
static void
update_trails( void ) {
  struct trail_t *t;
  struct trail_t *prev;

  prev = &app.trail_head;

  for ( t = app.trail_head.next; t != NULL; t = t->next ) {
    Stds_TrailUpdate( t );

    if ( t->flags & DEATH_MASK ) {
      if ( t == app.trail_tail ) {
        app.trail_tail = prev;
      }

      prev->next = t->next;
      free( t );
      t = prev;
    }
    prev = t;
  }
}

/*
 *
 */
static void
update_parallax_backgrounds( void ) {
  struct parallax_background_t *p;
  for ( p = app.parallax_head.next; p != NULL; p = p->next ) {
    Stds_ParallaxBackgroundUpdate( p );
  }
}

/*
 *
 */
static void
update_enemies( void ) {
  struct entity_t *e;
  for ( e = stage.enemy_head.next; e != NULL; e = e->next ) {
    enum CollisionSide s = Stds_CheckAABBCollision( player, e );

    if ( s == SIDE_TOP || s == SIDE_BOTTOM ) {
      player->dy = 0;
    }

    if ( s == SIDE_LEFT || s == SIDE_RIGHT ) {
      player->dx = 0;
    }
  }
}

/*
 *
 */
static void
draw( void ) {
  SDL_Color c = Stds_CombineFadeColor( &f );
  draw_parallax_backgrounds();
  Stds_ParticleSystemDraw( ps );
  Stds_DrawRectStroke( 0, 0, app.SCREEN_WIDTH, app.SCREEN_HEIGHT, 8, &c, 0xff );
  draw_trails();
  draw_enemies();
  player_draw();

  Stds_DrawLineGrid( grid );
}

/**
 *
 */
static void
draw_trails( void ) {
  struct trail_t *t;
  for ( t = app.trail_head.next; t != NULL; t = t->next ) {
    Stds_TrailDraw( t );
  }
}

/**
 *
 */
static void
draw_parallax_backgrounds( void ) {
  struct parallax_background_t *p;
  for ( p = app.parallax_head.next; p != NULL; p = p->next ) {
    Stds_ParallaxBackgroundDraw( p );
  }
}

/**
 *
 */
static void
draw_enemies( void ) {
  struct entity_t *e;
  for ( e = stage.enemy_head.next; e != NULL; e = e->next ) {
    enemy_draw( e );
  }
}

/**
 *
 */
static void
cleanup_stage( void ) {
  SDL_LogDebug( SDL_LOG_CATEGORY_APPLICATION, "Freeing player.\n" );
  free( player );

  Stds_FreeGrid( grid );
}

/**
 *
 */
static void
add_particles( int32_t x, int32_t y, size_t n ) {
  for ( uint32_t i = 0; i < n; i++ ) {
    struct particle_t p;

    /* Adds an animated particle. */
    p            = animated_particle_init( x, y );
    int32_t code = Stds_InsertParticle( ps, &p );
    if ( code == PS_FULL ) {
      return;
    }
  }
}
