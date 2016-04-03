function [problem] = min(primal_vars, primal_constrained_vars)
% MIN  Creates a linearly constrained primal problem from the specified variables

    problem.type = 'min';
    problem.primal_vars = primal_vars;
    problem.primal_constrained_vars = primal_constrained_vars;
    problem.num_primals = prod(size(primal_vars));
    problem.num_constrained_primals = prod(size(primal_constrained_vars));
       
    % compute primal indices
    primal_idx = 0;
    for i=1:problem.num_primals
        primal_vars{i}.idx = primal_idx;
        
        sub_idx = primal_idx;
        num_subvars = prod(size(primal_vars{i}.subvars));
        for j=1:num_subvars
            primal_vars{i}.subvars{j}.idx = sub_idx;
            sub_idx = sub_idx + primal_vars{i}.subvars{j}.dim;
        end
        
        primal_idx = primal_idx + primal_vars{i}.dim;
    end
    
    % compute primal constrained indices
    primal_constrained_idx = 0;
    for i=1:problem.num_constrained_primals
        primal_constrained_vars{i}.idx = primal_constrained_idx;
        
        sub_idx = primal_constrained_idx;
        num_subvars = prod(size(primal_constrained_vars{i}.subvars));
        for j=1:num_subvars
            primal_constrained_vars{i}.subvars{j}.idx = sub_idx;
            sub_idx = sub_idx + primal_constrained_vars{i}.subvars{j}.dim;
        end
        
        primal_constrained_idx = primal_constrained_idx + primal_constrained_vars{i}.dim;
    end

    problem.data.linop = {};
    problem.data.prox_g = {};
    problem.data.prox_f = {};
    problem.data.prox_gstar = {};
    problem.data.prox_fstar = {};

    for i=1:problem.num_primals
        
        num_subvars = prod(size(primal_vars{i}.subvars));
        has_subvar_prox = false;
        for j=1:num_subvars
            if ~isempty(primal_vars{i}.subvars{j}.fun)
                problem.data.prox_g{end + 1} = primal_vars{i}.subvars{j}.fun(...
                    primal_vars{i}.subvars{j}.idx, ...
                    primal_vars{i}.subvars{j}.dim);
                
                has_subvar_prox = true;
            end           
        end

        % add primal prox
        if ~isempty(primal_vars{i}.fun) && ~has_subvar_prox
            problem.data.prox_g{end + 1} = primal_vars{i}.fun(...
                primal_vars{i}.idx, primal_vars{i}.dim);
        end
        
        % add linop
        if ~isempty(primal_vars{i}.linop)
            num_pairs = prod(size(primal_vars{i}.pairing));
            for j=1:num_pairs
                problem.data.linop{end + 1} = primal_vars{i}.linop{j}(...
                    primal_vars{i}.pairing{j}.idx, ...
                    primal_vars{i}.idx, ...
                    primal_vars{i}.pairing{j}.dim, ...
                    primal_vars{i}.dim);
            end
        end

        for j=1:num_subvars
            if ~isempty(primal_vars{i}.subvars{j}.linop)
                num_pairs = prod(size(primal_vars{i}.subvars{j}.pairing));
                for k=1:num_pairs
                    problem.data.linop{end + 1} = primal_vars{i}.subvars{j}.linop{k}(...
                        primal_vars{i}.subvars{j}.pairing{k}.idx, ...
                        primal_vars{i}.idx, ...
                        primal_vars{i}.subvars{j}.pairing{k}.dim, ...
                        primal_vars{i}.dim);
                end
            end
        end
    end

    for i=1:problem.num_constrained_primals
        num_subvars = ...
            prod(size(primal_constrained_vars{i}.subvars));
        has_subvar_prox = false;

        for j=1:num_subvars
            if ~isempty(primal_constrained_vars{i}.subvars{j}.fun)
                problem.data.prox_f{end + 1} = primal_constrained_vars{i}.subvars{j}.fun(...
                    primal_constrained_vars{i}.subvars{j}.idx, ...
                    primal_constrained_vars{i}.subvars{j}.dim);
                
                has_subvar_prox = true;
            end
        end

        if ~isempty(primal_constrained_vars{i}.fun) && ~has_subvar_prox
            problem.data.prox_f{end + 1} = primal_constrained_vars{i}.fun(...
                primal_constrained_vars{i}.idx, primal_constrained_vars{i}.dim);
        end
    end

    problem.data.scaling = 'alpha';
    problem.data.scaling_alpha = 1;
    problem.data.scaling_left = 1;
    problem.data.scaling_right = 1;
    
end