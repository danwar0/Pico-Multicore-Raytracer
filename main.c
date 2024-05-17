#include <math.h>
#include <stdbool.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/irq.h"
#include "LCD/LCD_1in3.h"



typedef struct{
    double i, j, k;
}Vec3D;

typedef struct{
    double red, green, blue;
}Colour;

enum Material{
    DIFFUSE,
    MIRROR
};

typedef struct{
    Vec3D position;
    double radius;
    enum Material material;
    Colour colour;
}Sphere;

typedef struct{
    bool isHit;
    Vec3D position;
    Vec3D normal;
    enum Material material;
    Colour colour;
}Intersection;




#define ACCURACY 0.0001

uint16_t screenBuffer[57600];

Sphere spheres[] = {
    {{0,0,240},50.0,DIFFUSE,{1,0,0}},
    {{0,-10050,240},10000.0,DIFFUSE,{0,1,0}}
};

const int numOfSpheres = 2;

const Vec3D lightPos = {100,100,200};

const double focalLength = 240;




double getMagnitude(Vec3D v){
    return sqrt(v.i * v.i + v.j * v.j + v.k * v.k);
}

Vec3D normalised(Vec3D v){
    Vec3D out = v;
    double mag = getMagnitude(v);
    v.i /= mag;
    v.j /= mag;
    v.k /= mag;
    return out;
}

Vec3D add(Vec3D a, Vec3D b){
    Vec3D out = {a.i + b.i, a.j + b.j, a.k + b.k};
    return out;
}

Vec3D scale(Vec3D v, double scale){
    Vec3D out = {v.i * scale, v.j * scale, v.k * scale};
    return out;
}

double dot(Vec3D a, Vec3D b){
    return a.i * b.i + a.j * b.j + a.k * b.k;
}

Vec3D findVec(Vec3D a, Vec3D b){
    Vec3D out = {b.i - a.i, b.j - a.j, b.k - a.k};
    return out;
}

Vec3D findUnitVec(Vec3D a, Vec3D b){
    Vec3D out = {b.i - a.i, b.j - a.j, b.k - a.k};
    double mag = getMagnitude(out);
    out.i /= mag;
    out.j /= mag;
    out.k /= mag;
    return out;
}



double getSphereIntersectionT(Vec3D origin, Vec3D direction, Sphere sphere){
    double a = dot(direction, direction);
    Vec3D co = findVec(sphere.position, origin);
    double b = 2.0 * dot(direction, co);
    double c = dot(co, co) - sphere.radius * sphere.radius;

    double discriminant = b*b-4.0*a*c;

    if(discriminant < 0){
        return -1;
    }

    if(discriminant == 0){
        return (-b + sqrt(discriminant)) / (2.0 * a);
    } 

    double t1 = (-b + sqrt(discriminant)) / (2.0 * a);
    double t2 = (-b - sqrt(discriminant)) / (2.0 * a);

    if(t1 <= 0 && t2 <= 0){
        return -1;
    }

    if(t1 > 0 && t2 > 0){
        return (t1 < t2) ? t1 : t2;
    }

    if(t1 > 0){
        return t1;
    }

    return t2;
}



Intersection getSceneIntersection(Vec3D origin, Vec3D direction){
    Intersection output;
    output.isHit = false;
    
    double closestT = 999999.0;
    Sphere* closestSphere = NULL;

    for(int i = 0; i < numOfSpheres; i++){
        double currentT = getSphereIntersectionT(origin, direction, spheres[i]);
        if(currentT > 0 && currentT < closestT){
            closestT = currentT;
            closestSphere = &spheres[i];
        }
    }

    if(closestSphere != NULL){
        output.isHit = true;
        output.position = add(origin, scale(direction, closestT));
        output.normal = findUnitVec(closestSphere->position, output.position);
        output.material = closestSphere->material;
        output.colour = closestSphere->colour;
    }

    return output;
}



Colour scaleColour(Colour colour, double scale){
    Colour out = {colour.red*scale, colour.green*scale, colour.blue*scale};
    return out;
}












void drawPoint(int x, int y, uint16_t colour){
    if(x < 0 || y < 0 || x >= 240 || y >= 240){
        return;
    }
    screenBuffer[y*240+x] = colour;
}



uint16_t convertColour(Colour colour){
    uint8_t blue = (uint8_t) (colour.blue*31);
    if(blue > 31){
        blue = 31;
    }

    uint8_t red = (uint8_t) (colour.red*31); 
    if(red > 31){
        red = 31;
    }

    uint8_t green = (uint8_t) (colour.green*63);
    if(green > 63){
        green = 63;
    }

    uint16_t col = (blue << 8) | (red << 3) | (green >> 3) | ((green & 0x07) << 13);

    return col;
}



Colour getPixelColour(double focalLength, double x, double y){
    Vec3D screenPos = {x,y,focalLength};
    Vec3D ray = normalised(screenPos);
    Vec3D origin = {0,0,0};
    
    
    Intersection firstIntersection = getSceneIntersection(origin, ray);
    
    if(firstIntersection.isHit){
        Vec3D shadowRay = findUnitVec(firstIntersection.position,lightPos);

        Intersection nextIntersection = getSceneIntersection(add(firstIntersection.position, scale(firstIntersection.normal,ACCURACY)),shadowRay);

        double portionOfLight = (dot(firstIntersection.normal,shadowRay)+1.0)/2.0;
        Colour col = scaleColour(firstIntersection.colour,portionOfLight);

        if(nextIntersection.isHit){
            //in shadow
            return scaleColour(col,0.8);
        }else{
            //lit
            return col;
        }


    }

    Colour skyColour = {0,0,0};
    return skyColour;
}

















bool core1Finished = false;

void core1Code(){
    for(int j = 120; j < 240; j++){
        for(int i = 0; i < 240; i++){
            double x = i-120;
            double y = 120-j;

            drawPoint(i,j,convertColour(getPixelColour(focalLength,x,y)));
        }
    }

    core1Finished = true;
}


int main(){
    DEV_Module_Init();
    LCD_1IN3_Init(HORIZONTAL);
    LCD_1IN3_Clear(0x0000);

    for(int i = 0; i < 57600; i++){
        screenBuffer[i] = 0xffff;
    }

    LCD_1IN3_Display(screenBuffer);

    multicore_launch_core1(core1Code);

    for(int j = 0; j < 120; j++){
        for(int i = 0; i < 240; i++){
            double x = i-120;
            double y = 120-j;

            drawPoint(i,j,convertColour(getPixelColour(focalLength,x,y)));
        }
        LCD_1IN3_Display(screenBuffer);
    }

    while(!core1Finished){
        LCD_1IN3_Display(screenBuffer);
    }

    return 0;
}