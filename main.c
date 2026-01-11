#include <X11/Xlib.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <unistd.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>

#define WIDTH  480
#define HEIGHT 800
#define NUM_AGENTS 2

#define PICK_RADIUS 0.9f
#define ROT_SENS    0.005f
#define ROT_DAMP    0.96f

/* loosened movement bounds */
#define LEFT_BOUND  -4.0f
#define RIGHT_BOUND  4.0f
#define BOTTOM_BOUND -3.0f
#define TOP_BOUND     3.0f

/* ================= SHADERS ================= */

const char *vs_src =
"attribute vec3 aPos;\n"
"attribute vec3 aColor;\n"
"attribute vec3 aNormal;\n"
"uniform mat4 uMVP;\n"
"uniform mat4 uWorld;\n"
"varying vec3 vColor;\n"
"varying vec3 vNormal;\n"
"void main(){\n"
"  vColor = aColor;\n"
"  vNormal = mat3(uWorld) * aNormal;\n"
"  gl_Position = uMVP * vec4(aPos,1.0);\n"
"}\n";

const char *fs_src =
"precision mediump float;\n"
"varying vec3 vColor;\n"
"varying vec3 vNormal;\n"
"uniform float uSelected;\n"
"void main(){\n"
"  vec3 N = normalize(vNormal);\n"
"  vec3 L = normalize(vec3(-0.4,-1.0,-0.6));\n"
"  vec3 V = vec3(0.0,0.0,1.0);\n"
"  float d = max(dot(N,-L),0.0);\n"
"  vec3 H = normalize(-L+V);\n"
"  float s = pow(max(dot(N,H),0.0),24.0);\n"
"  vec3 base = vColor*(0.25+d*0.75) + vec3(s*0.35);\n"
"  vec3 highlight = mix(base, vec3(1.0,1.0,0.3), uSelected);\n"
"  gl_FragColor = vec4(highlight,1.0);\n"
"}\n";

/* ================= MATH ================= */

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
    m[5]=cosf(a); m[6]=sinf(a);
    m[9]=-sinf(a); m[10]=cosf(a);
}
void mat4_rotate_y(float *m,float a){
    mat4_identity(m);
    m[0]= cosf(a); m[2]=-sinf(a);
    m[8]= sinf(a); m[10]=cosf(a);
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
void mat4_perspective(float *m,float fov,float asp,float n,float f){
    float t=tanf(fov*0.5f);
    for(int i=0;i<16;i++) m[i]=0;
    m[0]=1/(asp*t);
    m[5]=1/t;
    m[10]=-(f+n)/(f-n);
    m[11]=-1;
    m[14]=-(2*f*n)/(f-n);
}

/* ================= DRAW ================= */

void draw_cube(GLint uMVP,GLint uWorld,
               float *proj,float *view,float *model){
    float t2[16],mvp[16];
    mat4_mul(t2,view,model);
    mat4_mul(mvp,proj,t2);
    glUniformMatrix4fv(uWorld,1,GL_FALSE,model);
    glUniformMatrix4fv(uMVP,1,GL_FALSE,mvp);
    glDrawArrays(GL_TRIANGLES,0,36);
}

/* ================= AGENT ================= */

typedef struct{
    float x,y;
    float rot;
    float rot_vel;
} Agent;

Agent agents[NUM_AGENTS];

/* ================= GL ================= */

GLuint compile(GLenum t,const char *s){
    GLuint sh=glCreateShader(t);
    glShaderSource(sh,1,&s,NULL);
    glCompileShader(sh);
    return sh;
}

/* ================= MAIN ================= */

int main(){
    Display *xd=XOpenDisplay(NULL);
    Window win=XCreateSimpleWindow(xd,DefaultRootWindow(xd),0,0,WIDTH,HEIGHT,0,0,0);
    XSelectInput(xd,win,ButtonPressMask|ButtonReleaseMask|PointerMotionMask);
    XMapWindow(xd,win);

    EGLDisplay ed=eglGetDisplay((EGLNativeDisplayType)xd);
    eglInitialize(ed,NULL,NULL);

    EGLConfig cfg; EGLint n;
    EGLint cfg_attr[]={EGL_RENDERABLE_TYPE,EGL_OPENGL_ES2_BIT,EGL_SURFACE_TYPE,EGL_WINDOW_BIT,EGL_DEPTH_SIZE,16,EGL_NONE};
    eglChooseConfig(ed,cfg_attr,&cfg,1,&n);

    EGLSurface surf=eglCreateWindowSurface(ed,cfg,(EGLNativeWindowType)win,NULL);
    EGLContext ctx=eglCreateContext(ed,cfg,EGL_NO_CONTEXT,
        (EGLint[]){EGL_CONTEXT_CLIENT_VERSION,2,EGL_NONE});
    eglMakeCurrent(ed,surf,surf,ctx);

    glEnable(GL_DEPTH_TEST);

    GLuint prog=glCreateProgram();
    glAttachShader(prog,compile(GL_VERTEX_SHADER,vs_src));
    glAttachShader(prog,compile(GL_FRAGMENT_SHADER,fs_src));
    glBindAttribLocation(prog,0,"aPos");
    glBindAttribLocation(prog,1,"aColor");
    glBindAttribLocation(prog,2,"aNormal");
    glLinkProgram(prog);
    glUseProgram(prog);

    GLint uMVP=glGetUniformLocation(prog,"uMVP");
    GLint uWorld=glGetUniformLocation(prog,"uWorld");
    GLint uSelected=glGetUniformLocation(prog,"uSelected");

    /* ----- cube geometry ----- */
    float cube[]={
        -0.5,-0.5,0.5,1,0,0,0,0,1,   0.5,-0.5,0.5,1,0,0,0,0,1,   0.5,0.5,0.5,1,0,0,0,0,1,
        -0.5,-0.5,0.5,1,0,0,0,0,1,   0.5,0.5,0.5,1,0,0,0,0,1,  -0.5,0.5,0.5,1,0,0,0,0,1,
        -0.5,-0.5,-0.5,0,1,0,0,0,-1, -0.5,0.5,-0.5,0,1,0,0,0,-1, 0.5,0.5,-0.5,0,1,0,0,0,-1,
        -0.5,-0.5,-0.5,0,1,0,0,0,-1,  0.5,0.5,-0.5,0,1,0,0,0,-1, 0.5,-0.5,-0.5,0,1,0,0,0,-1,
        -0.5,-0.5,-0.5,0,0,1,-1,0,0, -0.5,-0.5,0.5,0,0,1,-1,0,0, -0.5,0.5,0.5,0,0,1,-1,0,0,
        -0.5,-0.5,-0.5,0,0,1,-1,0,0, -0.5,0.5,0.5,0,0,1,-1,0,0, -0.5,0.5,-0.5,0,0,1,-1,0,0,
         0.5,-0.5,-0.5,1,1,0,1,0,0,  0.5,0.5,-0.5,1,1,0,1,0,0,  0.5,0.5,0.5,1,1,0,1,0,0,
         0.5,-0.5,-0.5,1,1,0,1,0,0,  0.5,0.5,0.5,1,1,0,1,0,0,  0.5,-0.5,0.5,1,1,0,1,0,0,
        -0.5,0.5,-0.5,0,1,1,0,1,0, -0.5,0.5,0.5,0,1,1,0,1,0,  0.5,0.5,0.5,0,1,1,0,1,0,
        -0.5,0.5,-0.5,0,1,1,0,1,0,  0.5,0.5,0.5,0,1,1,0,1,0,  0.5,0.5,-0.5,0,1,1,0,1,0,
        -0.5,-0.5,-0.5,1,0,1,0,-1,0, 0.5,-0.5,-0.5,1,0,1,0,-1,0, 0.5,-0.5,0.5,1,0,1,0,-1,0,
        -0.5,-0.5,-0.5,1,0,1,0,-1,0, 0.5,-0.5,0.5,1,0,1,0,-1,0,-0.5,-0.5,0.5,1,0,1,0,-1,0
    };

    GLuint vbo;
    glGenBuffers(1,&vbo);
    glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof(cube),cube,GL_STATIC_DRAW);

    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,9*sizeof(float),(void*)0);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,9*sizeof(float),(void*)(3*sizeof(float)));
    glVertexAttribPointer(2,3,GL_FLOAT,GL_FALSE,9*sizeof(float),(void*)(6*sizeof(float)));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    /* ----- grid geometry ----- */
    float grid[]={
        -5,0,0, 0.8,0.2,0.2,  5,0,0, 0.8,0.2,0.2,
         0,-5,0, 0.2,0.8,0.2, 0,5,0, 0.2,0.8,0.2,
         0,0,-5, 0.2,0.2,0.8, 0,0,5, 0.2,0.2,0.8
    };

    GLuint grid_vbo;
    glGenBuffers(1,&grid_vbo);
    glBindBuffer(GL_ARRAY_BUFFER,grid_vbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof(grid),grid,GL_STATIC_DRAW);

    float proj[16],view[16];
    mat4_perspective(proj,1.1f,(float)WIDTH/HEIGHT,0.1f,50);

    float rx[16],ry[16],tr[16],tmp[16];
    mat4_rotate_x(rx,-0.6f);
    mat4_rotate_y(ry, 0.6f);
    mat4_translate(tr,0,-0.6f,-7.5f);
    mat4_mul(tmp,ry,rx);
    mat4_mul(view,tr,tmp);

    agents[0].x=-1.3f; agents[0].y=0;
    agents[1].x= 1.3f; agents[1].y=0;

    int grabbed=-1, selected=-1, last_x=0, last_y=0;

    while(1){
        while(XPending(xd)){
            XEvent e; XNextEvent(xd,&e);
            /* expanded screen -> world mapping */
            float wx=(float)e.xmotion.x/WIDTH*8.0f-4.0f;
            float wy=(float)(HEIGHT-e.xmotion.y)/HEIGHT*6.0f-3.0f;

            if(e.type==ButtonPress){
                last_x=e.xbutton.x;
                last_y=e.xbutton.y;
                selected=-1;
                grabbed=-1;
                for(int i=0;i<NUM_AGENTS;i++){
                    if(fabsf(wx-agents[i].x)<PICK_RADIUS &&
                       fabsf(wy-agents[i].y)<PICK_RADIUS){
                        selected=i;
                        grabbed=i;
                        break;
                    }
                }
            }

            if(e.type==ButtonRelease) grabbed=-1;

            if(e.type==MotionNotify && grabbed!=-1){
                int dx=e.xmotion.x-last_x;
                last_x=e.xmotion.x;
                agents[grabbed].x=wx;
                agents[grabbed].y=wy;
                if(agents[grabbed].x<LEFT_BOUND) agents[grabbed].x=LEFT_BOUND;
                if(agents[grabbed].x>RIGHT_BOUND) agents[grabbed].x=RIGHT_BOUND;
                if(agents[grabbed].y<BOTTOM_BOUND) agents[grabbed].y=BOTTOM_BOUND;
                if(agents[grabbed].y>TOP_BOUND) agents[grabbed].y=TOP_BOUND;
                agents[grabbed].rot_vel+=dx*ROT_SENS;
            }
        }

        for(int i=0;i<NUM_AGENTS;i++){
            agents[i].rot+=agents[i].rot_vel;
            agents[i].rot_vel*=ROT_DAMP;
        }

        glClearColor(0.05f,0.05f,0.08f,1);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

        /* draw grid */
        glBindBuffer(GL_ARRAY_BUFFER,grid_vbo);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)0);
        glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)(3*sizeof(float)));
        glDisableVertexAttribArray(2);
        glUniform1f(uSelected,0.0f);

        float ident[16], gv[16], gmvp[16];
        mat4_identity(ident);
        mat4_mul(gv,view,ident);
        mat4_mul(gmvp,proj,gv);
        glUniformMatrix4fv(uWorld,1,GL_FALSE,ident);
        glUniformMatrix4fv(uMVP,1,GL_FALSE,gmvp);
        glDrawArrays(GL_LINES,0,6);

        /* restore cube state */
        glBindBuffer(GL_ARRAY_BUFFER,vbo);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,9*sizeof(float),(void*)0);
        glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,9*sizeof(float),(void*)(3*sizeof(float)));
        glVertexAttribPointer(2,3,GL_FLOAT,GL_FALSE,9*sizeof(float),(void*)(6*sizeof(float)));
        glEnableVertexAttribArray(2);

        for(int i=0;i<NUM_AGENTS;i++){
            float root[16],rot[16],scale[16],tmp2[16],model[16];
            mat4_translate(root,agents[i].x,agents[i].y,0);
            mat4_rotate_y(rot,agents[i].rot);
            mat4_scale(scale,0.8f,1.2f,0.8f);
            mat4_mul(tmp2,rot,scale);
            mat4_mul(model,root,tmp2);
            glUniform1f(uSelected,(i==selected)?1.0f:0.0f);
            draw_cube(uMVP,uWorld,proj,view,model);
        }

        eglSwapBuffers(ed,surf);
        usleep(16000);
    }
}
