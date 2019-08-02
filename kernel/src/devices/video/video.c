#include <devices.h>

#define NTEXTURE 16384
#define NSPRITE  16384

static sem_t fb_sem;
extern uint8_t TERM_FONT[];

static void texture_fill(struct texture *tx, int top, uint8_t *bits, uint32_t fg, uint32_t bg) {
  //printf("texure_fill\n");
  uint32_t *px = tx->pixels;
  for (int y = 0; y < TEXTURE_H; y++)
    for (int x = 0; x < TEXTURE_W; x++) {
      int bitmask = top ? bits[y + TEXTURE_H] : bits[y];
      *px++ = ((bitmask >> (7 - x)) & 1) ? fg : bg;
      //printf("1");
    }
}

static void font_load(fb_t *fb, uint8_t *font) {
  //printf("font_load\n");
  for (int ch = 0; ch < 256; ch++) {
    //printf("texture_fill: %d\n", ch);
    texture_fill(&fb->textures[ch * 2 + 1], 0, &font[16 * ch], 0xffffff, 0x000000);
    texture_fill(&fb->textures[ch * 2 + 2], 1, &font[16 * ch], 0xffffff, 0x000000);
  }
  //printf("font_load2\n");
}

int fb_init(device_t *dev) {
  //printf("fb_init\n");
  fb_t *fb = dev->ptr;
//  printf("true fb ops:%p\n", fb->ops);
  fb->info = pmm->alloc(sizeof(struct display_info));
  fb->textures = pmm->alloc(sizeof(struct texture) * NTEXTURE);
  fb->sprites = pmm->alloc(sizeof(struct sprite) * NSPRITE);
  //printf("fb_init2\n");
  *(fb->info) = (struct display_info) {
    .width = screen_width(),
    .height = screen_height(),
    .num_displays = 8,
    .num_textures = NTEXTURE,
    .num_sprites = NSPRITE,
    .current = 0,
  };
  //printf("fb_init3\n");
  //printf("screen width: %x, screen height:%x \n", screen_width(), screen_height());
  kmt->sem_init(&fb_sem, dev->name, 1);
  //printf("fb_init4\n");
  font_load(fb, TERM_FONT);
  //printf("fb_init\n");
  return 0;
}

ssize_t fb_read(device_t *dev, off_t offset, void *buf, size_t count) {
  fb_t *fb = dev->ptr;
  if (offset != 0) return 0;
  if (count != sizeof(struct display_info)) return 0;
  memcpy(buf, fb->info, sizeof(struct display_info));
  return 0;
}

ssize_t fb_write(device_t *dev, off_t offset, const void *buf, size_t count) {
  printf("fb_write\n");
  fb_t *fb = dev->ptr;
  kmt->sem_wait(&fb_sem);
  
  if (offset == 0) {
    const struct display_info *info = buf;
    if (fb->info->current != info->current) {
      fb->info->current = info->current;
    }
  } else if (offset < SPRITE_BRK) {
    memcpy(((uint8_t *)fb->textures) + offset, buf, count);
  } else {
    // TODO: not remove stale sprites, and does not consider z-values
    _DEV_VIDEO_FBCTL_t ctl = {
      .w = TEXTURE_W,
      .h = TEXTURE_H,
    };
    for (struct sprite *sp = (struct sprite *)buf; sp < (struct sprite *)(buf + count); sp++) {
      if (sp->texture > 0 && sp->display == fb->info->current) {
        ctl.x = sp->x;
        ctl.y = sp->y;
        ctl.pixels = fb->textures[sp->texture].pixels;
        //printf("fb_write\n");
        _io_write(_DEV_VIDEO, _DEVREG_VIDEO_FBCTL, &ctl, sizeof(ctl));
      }
    }
  }
  kmt->sem_signal(&fb_sem);
  return count;
}

devops_t fb_ops = {
  .init = fb_init,
  .read = fb_read,
  .write = fb_write,
};
