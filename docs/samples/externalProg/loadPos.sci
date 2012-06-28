//Example function for loading, manipulating and writing 
// pos files in scilab, for integrating with 3Depict's 
// external program filter


//-------
function [errState, x,y,z,m]=loadPos(filename)

x=[];y=[]; z=[]; m=[];

//get filesize
[fileInf,ierr] = fileinfo(filename)
filesize=fileInf(1);

//ensure filesize has 16 as a factor.
if ( modulo(filesize,16) ~= 0 ) 
    errState=1;
    return
end

numEntries=filesize/16;

//Open the file for read only, in binary mode
[fd, err] = mopen(filename,'rb');

//Check to see we are A-OK
if err ~= 0
    errState=2;
    return
end    

//Read the data in as floating point values in big-endian format
data=mget(numEntries,'fb',fd);

//check read OK
if merror(fd)
   errState=3;
   mclose(fd);
   return
end

//Unsplice data, which was stored as xyzmxyzmxyzm...
x=data(1:4:$)';
y=data(2:4:$)';
z=data(3:4:$)';
m=data(4:4:$)';

clear data;

mclose(fd)

errState=0;

endfunction

function err=writePos(filename,x,y,z,m)
    //Check that the array sizes match
    sizes = [ length(x), length(y),length(z),length(m)];
    if max(sizes) ~= min(sizes) 
        err=1;
        return
    end
    
    //Open the file write, in binary mode
    [fd, errState] = mopen(filename,'wb');
    
    if(errState)
        err=2;
        return;
    end
    
    //Build a matrix to dump the data into 
    // in xyzmxyzmxyzm form 
    data=zeros(sizes(1)*4,1);
    data(1:4:$) = x;
    data(2:4:$) = y;
    data(3:4:$) = z;
    data(4:4:$) = m;     
 
    mput(data,'fb',fd);
    
    //Check for io error
    if merror(fd) ~=0
        mclose(fd);
        err=3;
        return;
    end
    
    err=0;
    mclose(fd);
endfunction

//-------

//START OF SCRIPT

//Inform scilab we may need lots of ram.
stacksize('max'); 

//Strip out the script arguments from the general scilab arguments
argsArray=sciargs();
realArgs=[];
numArgs =length(length(argsArray)); //'cause length() is dumb on strings.
for i=1:numArgs
    if argsArray(i) == '-args' & i != length(argsArray);
        realArgs=argsArray(i+1:$);
    end
end

if( length(argsArray) == 0)
    error('no file to open!');
end

//Load the first argument
[errState, x, y, z, m] = loadPos(realArgs(1));
if errState
    error( strcat(['Unable to load posfile, :( ' realArgs(1)]));
else
    printf('Opened file: %s ',realArgs(1));
end


//Draw the point cloud
scf
drawlater
plot3d1(x,y,z)
f=gcf();
pointCloud=f.children.children;
pointCloud.surface_mode="off";
pointCloud.mark_mode="on";
drawnow


//plot a histogram of m, avoiding the error where m has no span
// by artifically adding two elements, if needed.
scf();
if max(m) ~= min(m)
    histplot(100,m);
else
    histplot(100,[min(m)-1; m; max(m)+1]);
    
end


//Now shift each point around 
x=x-1;
y=y-1;
z=z-1;


//now write the file back
err=writePos('output.pos',x,y,z,m);
if  err~= 0
    error('failed to write posfile, :(');
end

//Kill Scilab, because were done and would like to go back to 3Depict.
exit

