function ex_rof_callback(K, f, lmb, it, x, y)
    % isotropic TV
    [m, n] = size(K);
    grad = K * x;
    gradnorms = sqrt(grad(1:n).^2 + grad(n+1:end).^2);
    en_prim = 0.5 * sum((x-f).^2) + lmb * sum(gradnorms);
        
    div = K' * y;
    en_dual = f' * div - 0.5 * sum(div.^2);
        
    en_gap = en_prim - en_dual;
    
    fprintf('it %5d: en_prim=%E, en_dual=%E, en_gap=%E\n', ...
            it, en_prim, en_dual, en_gap);
end