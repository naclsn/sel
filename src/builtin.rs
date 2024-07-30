use crate::value::{Type, Boundedness};

pub fn lookup(name: &str, types: &mut Vec<Type>) -> Result<(), ()> {
    if "input" == name {
        types.push(Type::Bytes(Boundedness::Infinite));
        Ok(())
    } else if "slice" == name {
        // TODO: boundedness needs to be a type in of itself so it can propagate the same way
        // slice :: Str -> Str* -> [Str*]*
        types.push(Type::Bytes(Boundedness::Infinite)); // here
        let strinf = types.len()-1;
        types.push(Type::List(Boundedness::Infinite, strinf)); // and here
        let lstinfstrinf = types.len()-1;
        types.push(Type::Func(strinf, lstinfstrinf));
        let blablablablabla = types.len()-1;
        types.push(Type::Func(1/*Str*/, blablablablabla));
        Ok(())
    } else {
        Err(())
    }
}
