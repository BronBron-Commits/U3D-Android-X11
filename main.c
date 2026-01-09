#include <X11/Xlib.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <unistd.h>
#include <math.h>
#include <stdbool.h>

#define WIDTH  480
#define HEIGHT 800

#define ROT_SENS   0.0025f
#define DAMPING    0.985f
#define MAX_VEL    0.15f

const char *vs_src =
"attribute vec3 aPos;\n"
"attribute vec3 aColor;\n"
"uniform mat4 uMVP;\n"
"varying vec3 vColor;\n"
"void main() {\n"
"  vColor = aColor;\n"
"  gl_Position = uMVP * vec4(aPos, 1.0);\n"
"}\n";

const char *fs_src =
"precision mediump float;\n"
"varying vec3 vColor;\n"
"void main() {\n"
"  gl_FragColor = vec4(vColor, 1.0);\n"
"}\n";

GLuint compile(GLenum t, const char *s) {
    GLuint sh = glCreateShader(t);
    glShaderSource(sh, 1, &s, NULL);
    glCompileShader(sh);
    return sh;
}

/* ---------- math ---------- */

void mat4_identity(float *m){
    for(int i=0;i<16;i++) m[i]=0;
    m[0]=m[5]=m[10]=m[15]=1;
}

void mat4_translate(float *m,float x,float y,float z){
    mat4_identity(m);
    m[12]=x; m[13]=y; m[14]=z;
}

void mat4_scale(float *m,float x,float y,float z){
    mat4_identity(m);
    m[0]=x; m[5]=y; m[10]=z;
}

void mat4_rotate_x(float *m,float a){
    mat4_identity(m);
    m[5]=cosf(a);  m[6]=sinf(a);
    m[9]=-sinf(a); m[10]=cosf(a);
}

void mat4_rotate_y(float *m,float a){
    mat4_identity(m);
    m[0]=cosf(a);  m[2]=-sinf(a);
    m[8]=sinf(a);  m[10]=cosf(a);
}

void mat4_perspective(float *m,float fov,float asp,float n,float f){
    float t=tanf(fov*0.5f);
    for(int i=0;i<16;i++) m[i]=0;
    m[0]=1/(asp*t);
    m[5]=1/t;
    m[10]=-(f+n)/(f-n);
    m[11]=-1;
    m[14]=-(2*f*n)/(f-n);
}

void mat4_mul(float *o,float *a,float *b){
    for(int c=0;c<4;c++)
        for(int r=0;r<4;r++)
            o[c*4+r]=
                a[0*4+r]*b[c*4+0]+
                a[1*4+r]*b[c*4+1]+
                a[2*4+r]*b[c*4+2]+
                a[3*4+r]*b[c*4+3];
}

float clamp(float v,float mn,float mx){
    if(v<mn) return mn;
    if(v>mx) return mx;
    return v;
}

/* ---------- draw ---------- */

void draw_cube(GLint uMVP,float *proj,float *view,float *world,float *model){
    float tmp1[16],tmp2[16],mvp[16];
    mat4_mul(tmp1,world,model);
    mat4_mul(tmp2,view,tmp1);
    mat4_mul(mvp,proj,tmp2);
    glUniformMatrix4fv(uMVP,1,GL_FALSE,mvp);
    glDrawArrays(GL_TRIANGLES,0,36);
}

/* ---------- main ---------- */

int main(){
    Display *xd=XOpenDisplay(NULL);
    Window win=XCreateSimpleWindow(xd,DefaultRootWindow(xd),0,0,WIDTH,HEIGHT,0,0,0);
    XSelectInput(xd,win,ButtonPressMask|ButtonReleaseMask|PointerMotionMask);
    XMapWindow(xd,win);

    EGLDisplay ed=eglGetDisplay((EGLNativeDisplayType)xd);
    eglInitialize(ed,NULL,NULL);

    EGLint cfg_attr[]={EGL_RENDERABLE_TYPE,EGL_OPENGL_ES2_BIT,EGL_SURFACE_TYPE,EGL_WINDOW_BIT,EGL_DEPTH_SIZE,16,EGL_NONE};
    EGLConfig cfg; EGLint n;
    eglChooseConfig(ed,cfg_attr,&cfg,1,&n);

    EGLSurface surf=eglCreateWindowSurface(ed,cfg,(EGLNativeWindowType)win,NULL);
    EGLint ctx_attr[]={EGL_CONTEXT_CLIENT_VERSION,2,EGL_NONE};
    EGLContext ctx=eglCreateContext(ed,cfg,EGL_NO_CONTEXT,ctx_attr);
    eglMakeCurrent(ed,surf,surf,ctx);

    glViewport(0,0,WIDTH,HEIGHT);
    glEnable(GL_DEPTH_TEST);

    GLuint prog=glCreateProgram();
    glAttachShader(prog,compile(GL_VERTEX_SHADER,vs_src));
    glAttachShader(prog,compile(GL_FRAGMENT_SHADER,fs_src));
    glLinkProgram(prog);
    glUseProgram(prog);

    float verts[] = {
        -0.5,-0.5,0.5,1,0,0,  0.5,-0.5,0.5,1,0,0,  0.5,0.5,0.5,1,0,0,
        -0.5,-0.5,0.5,1,0,0,  0.5,0.5,0.5,1,0,0, -0.5,0.5,0.5,1,0,0,
        -0.5,-0.5,-0.5,0,1,0, -0.5,0.5,-0.5,0,1,0,  0.5,0.5,-0.5,0,1,0,
        -0.5,-0.5,-0.5,0,1,0,  0.5,0.5,-0.5,0,1,0,  0.5,-0.5,-0.5,0,1,0,
        -0.5,-0.5,-0.5,0,0,1, -0.5,-0.5,0.5,0,0,1, -0.5,0.5,0.5,0,0,1,
        -0.5,-0.5,-0.5,0,0,1, -0.5,0.5,0.5,0,0,1, -0.5,0.5,-0.5,0,0,1,
         0.5,-0.5,-0.5,1,1,0,  0.5,0.5,-0.5,1,1,0,  0.5,0.5,0.5,1,1,0,
         0.5,-0.5,-0.5,1,1,0,  0.5,0.5,0.5,1,1,0,  0.5,-0.5,0.5,1,1,0,
        -0.5,0.5,-0.5,0,1,1, -0.5,0.5,0.5,0,1,1,  0.5,0.5,0.5,0,1,1,
        -0.5,0.5,-0.5,0,1,1,  0.5,0.5,0.5,0,1,1,  0.5,0.5,-0.5,0,1,1,
        -0.5,-0.5,-0.5,1,0,1,  0.5,-0.5,-0.5,1,0,1,  0.5,-0.5,0.5,1,0,1,
        -0.5,-0.5,-0.5,1,0,1,  0.5,-0.5,0.5,1,0,1, -0.5,-0.5,0.5,1,0,1
    };

    GLuint vbo;
    glGenBuffers(1,&vbo);
    glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof(verts),verts,GL_STATIC_DRAW);

    GLint aPos=glGetAttribLocation(prog,"aPos");
    GLint aColor=glGetAttribLocation(prog,"aColor");
    glEnableVertexAttribArray(aPos);
    glVertexAttribPointer(aPos,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)0);
    glEnableVertexAttribArray(aColor);
    glVertexAttribPointer(aColor,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)(3*sizeof(float)));

    GLint uMVP=glGetUniformLocation(prog,"uMVP");

    float proj[16],view[16];
    mat4_perspective(proj,1.0f,(float)WIDTH/HEIGHT,0.1f,50);
    mat4_translate(view,0,0,-6);

    float world[16],rx[16],ry[16];
    float rot_x=0,rot_y=0,vel_x=0,vel_y=0;
    bool dragging=false; int lx=0,ly=0;

    /* body parts */
    float torso[16], head[16], armL[16], armR[16], legL[16], legR[16];
    float tmp[16];

    mat4_scale(torso,0.8f,1.2f,0.4f);

    mat4_translate(head,0,1.1f,0);
    mat4_scale(tmp,0.4f,0.4f,0.4f);
    mat4_mul(head,head,tmp);

    mat4_translate(armL,-0.7f,0.3f,0);
    mat4_scale(tmp,0.25f,0.8f,0.25f);
    mat4_mul(armL,armL,tmp);

    mat4_translate(armR,0.7f,0.3f,0);
    mat4_scale(tmp,0.25f,0.8f,0.25f);
    mat4_mul(armR,armR,tmp);

    mat4_translate(legL,-0.3f,-1.2f,0);
    mat4_scale(tmp,0.3f,0.9f,0.3f);
    mat4_mul(legL,legL,tmp);

    mat4_translate(legR,0.3f,-1.2f,0);
    mat4_scale(tmp,0.3f,0.9f,0.3f);
    mat4_mul(legR,legR,tmp);

    while(1){
        while(XPending(xd)){
            XEvent e; XNextEvent(xd,&e);
            if(e.type==ButtonPress){dragging=true;lx=e.xbutton.x;ly=e.xbutton.y;}
            if(e.type==ButtonRelease) dragging=false;
            if(e.type==MotionNotify && dragging){
                int dx=e.xmotion.x-lx;
                int dy=e.xmotion.y-ly;
                lx=e.xmotion.x; ly=e.xmotion.y;
                vel_y+=dx*ROT_SENS;
                vel_x+=dy*ROT_SENS;
                vel_x=clamp(vel_x,-MAX_VEL,MAX_VEL);
                vel_y=clamp(vel_y,-MAX_VEL,MAX_VEL);
            }
        }

        rot_x+=vel_x; rot_y+=vel_y;
        vel_x*=DAMPING; vel_y*=DAMPING;

        mat4_rotate_y(ry,rot_y);
        mat4_rotate_x(rx,rot_x);
        mat4_mul(world,ry,rx);

        glClearColor(0.05f,0.05f,0.08f,1);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

        draw_cube(uMVP,proj,view,world,torso);
        draw_cube(uMVP,proj,view,world,head);
        draw_cube(uMVP,proj,view,world,armL);
        draw_cube(uMVP,proj,view,world,armR);
        draw_cube(uMVP,proj,view,world,legL);
        draw_cube(uMVP,proj,view,world,legR);

        eglSwapBuffers(ed,surf);
        usleep(16000);
    }
}
