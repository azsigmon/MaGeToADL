#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <vector>
#include <fstream>

#include "ADLCluster.hh"

std::vector<int> HitsId;

ADLCluster::ADLCluster(int channel)
{
  if(channel >= 1000) detId = 0; //To account for non-gerda array cases
  else detId = channel;
  clustersPos.reserve(4);
}

ADLCluster::~ADLCluster()
{

}

typedef struct { double x, y; int group; } point_t, *point;

double randf(double m)
{
    return m * rand() / (RAND_MAX - 1.);
}

void ADLCluster::SortHitsId(int Nhits,int Npts,int*hIddet)
{
  HitsId.assign(Npts,0);
  int j = 0;

  for (int i=0;i<Nhits;i++) if(hIddet[i] == detId) {HitsId[j] = i; j++;}
}

point set_rz(int Npts, float* hr,float*hz)
{
  point p, pt = (point) malloc(sizeof(point_t) * Npts);
  int i =0;
  /* note: this is not a uniform 2-d distribution */
  for (p = pt + Npts; p-- > pt;) {
    p->x = hr[HitsId[i]];
    p->y = hz[HitsId[i]];
    //    printf("    Set data : %d %.03f %.03f \n",HitsId[i],hr[HitsId[i]],hz[HitsId[i]]);
    i++;
  }
  
  return pt;
}

double GetDist(double x0,double y0,double x1,double y1){
    double x = x0-x1;
    double y = y0-y1;
    return x*x+y*y;
}

double mean(double x,double y){
    return (x+y)/2.;
}

double dist2(point a, point b)
{
    double x = a->x - b->x, y = a->y - b->y;
    return x*x + y*y;
}

int nearest(point pt, point cent, int n_cluster, double *d2)
{
    int i, min_i;
    point c;
    double d, min_d;
    
#	define for_n for (c = cent, i = 0; i < n_cluster; i++, c++)
    for_n {
        min_d = HUGE_VAL;
        min_i = pt->group;
        for_n {
            if (min_d > (d = dist2(c, pt))) {
                min_d = d; min_i = i;
            }
        }
    }
    if (d2) *d2 = min_d;
    return min_i;
}

void kpp(point pts, int len, point cent, int n_cent)
{
#	define for_len for (j = 0, p = pts; j < len; j++, p++)
    int i, j;
    int n_cluster;
    double sum, *d = (double*)malloc(sizeof(double) * len);
    
    point p, c;
    cent[0] = pts[ rand() % len ];
    for (n_cluster = 1; n_cluster < n_cent; n_cluster++) {
        sum = 0;
        for_len {
            nearest(p, cent, n_cluster, d + j);
            sum += d[j];
        }
        sum = randf(sum);
        for_len {
            if ((sum -= d[j]) > 0) continue;
            cent[n_cluster] = pts[j];
            break;
        }
    }
    for_len p->group = nearest(p, cent, n_cluster, 0);
    free(d);
}

void lloyd(point pts, int len, int n_cluster,std::vector<std::vector<double> > &cluster, int detId)
{
    int i, j, min_i;
    int changed;
    
    point cent = (point)malloc(sizeof(point_t) * n_cluster), p, c;
    
    /* assign init grouping randomly */
    //for_len p->group = j % n_cluster;
    
    /* or call k++ init */
    kpp(pts, len, cent, n_cluster);
    
    do {
        /* group element for centroids are used as counters */
        for_n { c->group = 0; c->x = c->y = 0; }
        for_len {
            c = cent + p->group;
            c->group++;
            c->x += p->x; c->y += p->y;
        }
        for_n { c->x /= c->group; c->y /= c->group; }
        
        changed = 0;
        /* find closest centroid of each point */
        for_len {
            min_i = nearest(p, cent, n_cluster, 0);
            if (min_i != p->group) {
                changed++;
                p->group = min_i;
            }
        }
    } while (changed > (len >> 10)); /* stop when 99.9% of points are good */
    
    for_n { c->group = i; }
    
    for(i = 0; i < n_cluster; i++, cent++) {
        
        cluster[0][i] = cent->x;
        cluster[1][i] = cent->y;
        cluster[2][i] = detId;
        cluster[3][i] = 0.;     // will be filled afterwards
    }
}

int ADLCluster::CheckClusters(int Nhits,int Npts,int Ncls,float*hr,float* hz, double RMS){
    
    int Nin = 0, Nin2 = 0, Nout = 0;
    double dist;
    int IDin[Nhits];
    
    //printf("   Enter check clusters : %d / %d %d \n", Npts,HitsId.size(),Ncls);

    for(int j = 0;j<Nhits;j++) IDin[j] = 0;
    
    for(int i = 0;i<Ncls;i++){
        Nout = 0;
        for(int j = 0;j<Npts;j++){
	  if(IDin[HitsId[j]] == 0){
	      dist = sqrt(GetDist(hr[HitsId[j]],hz[HitsId[j]],clustersPos[0][i],clustersPos[1][i]));
                if(dist <= RMS){ Nin++; IDin[HitsId[j]] = 1;}
		//                else if(dist <= 1.5*RMS){ Nin2++; IDin[HitsId[j]] = 1;}
                else{ 
		  Nout++; 
		}
            }
        }
    }
                    
  //  printf("Counting : %d %d %.03f %.03f %.03f sum : %.02f \n",Ncls,Npts,double(Nin)/double(Npts),double(Nin2)/double(Npts),double(Nout)/double(Npts),double(Nin+Nin2+Nout)/double(Npts));

    //    if(double(Nin+Nin2)/double(Npts) > 0.99 && double(Nin)/double(Npts) < 0.99) return 2;
    //    else if(double(Nin)/double(Npts) < 0.99) return 0;
    if(double(Nin)/double(Npts) < 0.99) return 0;
   
    int IDoverlap[Ncls][Ncls];
    int overlap = 0;

    for(int i = 0;i<Ncls;i++)
        for(int j = i;j<Ncls;j++)
            IDoverlap[i][j] = 0;
    
    for(int i = 0;i<Ncls;i++){
        for(int j = i;j<Ncls;j++){
            if(j > i){
                dist = sqrt(GetDist(clustersPos[0][j],clustersPos[1][j],clustersPos[0][i],clustersPos[1][i]));
                if(dist < 2.*RMS){
                    IDoverlap[i][j] = 1;
                    overlap++;
                }
            }
        }
    }

    if(overlap>0){
      std::vector<double> newclusterX, newclusterY, newclusterID, newclusterE;
      double meanX, meanY;
    
        for(int i = 0;i<Ncls;i++){
            meanX = 0.;
            meanY = 0.;
            overlap = 0;
        
            for(int j = i;j<Ncls;j++){
	      if(IDoverlap[i][j] == 1 && clustersPos[0][j] != 666){
		meanX += mean(clustersPos[0][i],clustersPos[0][j]);
		meanY += mean(clustersPos[1][i],clustersPos[1][j]);
		clustersPos[0][j] = 666;
		clustersPos[1][j] = 666;
		overlap++;
	      }
            }
            if(overlap == 0 && clustersPos[0][i] != 666){
	      newclusterX.push_back(clustersPos[0][i]);
	      newclusterY.push_back(clustersPos[1][i]);
	      newclusterID.push_back(detId);
	      newclusterE.push_back(0.);    // will be filled afterwards
            }
            else if(clustersPos[0][i] != 666){
	      newclusterX.push_back(meanX/double(overlap));
	      newclusterY.push_back(meanY/double(overlap));
	      newclusterID.push_back(detId);
	      newclusterE.push_back(0.); 	  // will be filled afterwards
       	    }
    	}
	
	clustersPos.clear();
    	clustersPos.push_back(newclusterX);
    	clustersPos.push_back(newclusterY);
    	clustersPos.push_back(newclusterID);
    	clustersPos.push_back(newclusterE);
    }
    
    //printf("%lu clusters has been removed \n",Ncls-cluster[0].size());
    
    return 1;
}

void ADLCluster::SetClusterEnergy(int Npts,int Ncls,float*hr,float* hz,float*hEdep, double threshold)
{
    for(int i = 0;i<Ncls;i++){
        clustersPos[3][i] = 0.;
        for(int j = 0;j<Npts;j++)
            if(sqrt(GetDist(hr[HitsId[j]],hz[HitsId[j]],clustersPos[0][i],clustersPos[1][i])) < threshold) clustersPos[3][i] += hEdep[HitsId[j]];
    }
}

double ADLCluster::CheckEdep(int Npts, int Ncls, float* hEdep)
{
  double clusterEdep = 0;
  double hitsEdep = 0;

  for(int i = 0;i<Ncls;i++) clusterEdep += clustersPos[3][i];
  for(int i = 0;i<Npts;i++) hitsEdep += hEdep[HitsId[i]];

//  printf("Energy deposited in cluster %lf and hits %lf. \n",clusterEdep,hitsEdep);
//  printf("Ratio : %lf \n",clusterEdep/hitsEdep);

  return clusterEdep/hitsEdep;
}

void ADLCluster::GetRadCoord(int Npts,float* hx,float* hy)
{
  for(int i = 0; i<Npts;i++) hr[i] = sqrt(pow(hx[i],2)+pow(hy[i],2));
}

int ADLCluster::GetDetHits(int Nhits,int* hIddet){

  int NdetHits = 0;
  
  for(int i = 0;i<Nhits;i++) if(hIddet[i] == detId) NdetHits++;

  return NdetHits;
}

void CheckData(int Npts,point data)
{
  int i;
  point p;
  for(i = 0, p = data; i < Npts; i++, p++) printf("    Check data : %.04f %.04f \n",p->x,p->y);
}

double ADLCluster::LaunchClustering(int Nhits, float* hx,float* hy,float*hz,float* hEdep,int* hIddet)
{
  int debug = 0;
  int Ncls = 1;             // Initial number of clusters
  int Nstep = 6;           // Maximum number of cluster authorized
  int check = 0;            // Flag to determine the appropriate number of clusters
  int Npts;                 // Number of hits for each detector
  double threshold = 0.1;   // Cluster size in cm
  
  GetRadCoord(Nhits,hx,hy); // Transform (X,Y) coord to radial coord.
  Npts = GetDetHits(Nhits,hIddet);
  
  SortHitsId(Nhits,Npts,hIddet);
  point data = set_rz(Npts,hr,hz);
  //  CheckData(Npts,data);
  
  for(int i = 0;i<Nstep;i++){
    clustersPos.clear();
    clustersPos.assign(4,std::vector<double>(Ncls));

    if(debug) printf(" Number of cluster : %d x %d \n",clustersPos.size(),clustersPos[0].size());        
    if(debug) printf(" %d %d %d %d / %d \n",detId,Ncls,check, Npts,Nhits);
    
    lloyd(data, Npts, Ncls, clustersPos, detId);
    if(debug) printf(" lloyd step : %d \n",i);
    check = CheckClusters(Nhits,Npts,Ncls,hr,hz,threshold);
    if(debug) printf(" check step : %d \n",check);
    if(check == 1) break;
    else Ncls++;
  }
  if(debug) printf(" End of steps. Number of cluster : %d x %d \n",clustersPos.size(),clustersPos[0].size());
  
  Ncls = clustersPos[0].size();
  
  free(data);
  
  if(debug) printf("\n   Set %d cluster Edep \n",Ncls);
  SetClusterEnergy(Npts,Ncls,hr,hz,hEdep,threshold);
  if(debug) printf("   Check Edep \n");
  
  return CheckEdep(Npts,Ncls,hEdep);
}

std::vector<std::vector<double> > ADLCluster::GetClusters(){return clustersPos;}

