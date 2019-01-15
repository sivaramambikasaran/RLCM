% Check the results of Test_GP.sh

RandomFieldFileBasename = 'Test_GP_Standard_RF';
LogLikFileName = 'Test_GP_Standard_LogLik.txt';
KrigedRandomFieldFileBasename = 'Test_GP_Standard_RF_kriged';
PredictionsFileName = 'Test_GP_Standard_Pred.txt';

% Check random field
y = load([RandomFieldFileBasename '.val']);

fid = fopen([RandomFieldFileBasename '.info'], 'r');
d = fscanf(fid, '%d\n', 1);
tline = fgetl(fid); Dim = sscanf(tline, '%d')';
tline = fgetl(fid); Lower = sscanf(tline, '%g')';
tline = fgetl(fid); Upper = sscanf(tline, '%g')';
num_param = fscanf(fid, '%d\n', 1);
ell = fscanf(fid, '%g\n', 1);
nu = fscanf(fid, '%g\n', 1);
tau = fscanf(fid, '%g\n', 1);
lambda = 10^tau;
fclose(fid);

y_2D = reshape(y, Dim);
figure(1); imagesc(y_2D); title('RF');
figure(2); surf(y_2D); title('RF');

% Check loglik
M = load(LogLikFileName);

Num_ell = length(unique(M(:,1)));
Num_nu = length(unique(M(:,2)));
Num_tau = length(unique(M(:,3)));

L = reshape(M(:,4), [Num_tau, Num_nu, Num_ell]);
% Note that the matlab L has a different shape. To get the Matlab L,
% do permute(L,[3,2,1])

% Check kriged random field
y2 = load([KrigedRandomFieldFileBasename '.val']);

fid = fopen([KrigedRandomFieldFileBasename '.info'], 'r');
d = fscanf(fid, '%d\n', 1);
tline = fgetl(fid); Dim = sscanf(tline, '%d')';
tline = fgetl(fid); Lower = sscanf(tline, '%g')';
tline = fgetl(fid); Upper = sscanf(tline, '%g')';
num_param = fscanf(fid, '%d\n', 1);
hat_ell = fscanf(fid, '%g\n', 1);
hat_nu = fscanf(fid, '%g\n', 1);
hat_tau = fscanf(fid, '%g\n', 1);
hat_lambda = 10^hat_tau;
fclose(fid);

y2_2D = reshape(y2, Dim);
figure(3); imagesc(y2_2D); title('Kriged RF');
figure(4); surf(y2_2D); title('Kriged RF');

% Check predictions
M = load(PredictionsFileName);
ytest = M(:,1);
ytest_predict = M(:,2);
ytest_stddev = M(:,3);

err = ytest_predict - ytest;
[sorted_ytest_stddev, idx] = sort(ytest_stddev);
sorted_err = abs(err(idx));
figure(5);
plot(sorted_err, 'r.');
hold on;
plot(sorted_ytest_stddev*3, 'b');
legend('predict err', '3 * stderr');
hold off;
title('Prediction error');
